#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "MimeTypes.h"
#include "gif_encoder.h"
#include "gif_exception.h"
#include "gif_lsb.h"
#include "imsq.h"
#include "log.h"
#include "path.h"
#include "quantizer.h"

using std::string, std::vector, std::span;
using namespace GIFImage;
using namespace GIFLsb;
using namespace GIFEnc;

static constexpr size_t READ_CHUNK_SIZE = 1 << 20;  // 1 MiB, should be enough to fit header
// static const string ENCODED_FILE_NAME   = "mtk";
static constexpr double PROGRESS_STEP = 0.0314;  // 3.14%

// (1 << (3 * lsbLevel)) * numColors + 2 < 1 << minCodeLength
//                                     ^ these 2: header pixel & transparent color (if any)
// info-density: 3 * lsbLevel / minCodeLength
static std::pair<u32, u32>
getLsbLevelAndMinCodeLength(const u32 numColors) {
    if (numColors <= 3) {
        return {2, 8};
    } else if (numColors <= 7) {
        return {1, 6};
    } else if (numColors <= 15) {
        return {1, 7};
    } else if (numColors <= 31) {
        return {1, 8};
    } else {
        return {0, 8};
    }
}

static DitherMode
getDitherMode(const bool disableDither, const u32 frameCount, const bool grayscale) {
    // if disabled by user
    if (disableDither) {
        return DitherNone;
    }
    // if grayscale and more than 1 frame, use ordered dithering.
    // ordered dithering can prevent "snowy screen" effect,
    // but only works well with grayscale images
    else if (grayscale && frameCount > 1) {
        return DitherOrdered;
    }
    // if only 1 frame, use Floyd-Steinberg dithering for better quality
    else if (frameCount == 1) {
        return DitherFloydSteinberg;
    }
    // otherwise disable dithering
    else {
        return DitherNone;
    }
}

static std::shared_ptr<vector<QuantizerResult>>
quant(const ImageSequenceRef& image,
      const EncodeOptions& args,
      const u32 numColors,
      const DitherMode ditherMode,
      const u32 frameCount,
      const u32 width,
      const u32 height) {
    u32 quantizedFrameCount = 0;
    std::mutex cntMutex;
    auto resultsRef = std::make_shared<vector<QuantizerResult>>(frameCount);
    std::mutex mutex;
    vector<std::thread> threads;

    auto threadCount = args.threadCount;
    if (threadCount > frameCount) {
        GeneralLogger::warning("Number of threads is greater than number of frames. Reducing to " +
                               std::to_string(frameCount));
        threadCount = frameCount;
    }
    const u32 framePerThread = (frameCount + threadCount - 1) / threadCount;

    for (u32 i = 0; i < threadCount; ++i) {
        threads.push_back(std::thread([&, i]() {
            u32 startFrame = i * framePerThread;
            u32 endFrame   = startFrame + framePerThread;
            if (endFrame > frameCount) endFrame = frameCount;
            for (u32 j = startFrame; j < endFrame; ++j) {
                auto frameBuffer = image->getFrameBuffer(j, width, height, false);
                if (args.markText != "none") {
                    span<PixelBGRA> frameSpan(frameBuffer.data(), width * height);
                    ImageSequence::drawText(frameSpan, width, height, args.markText);
                }
                {
                    auto result = quantize(frameBuffer,
                                           width,
                                           height,
                                           numColors,
                                           ditherMode,
                                           args.grayscale,
                                           args.transparency,
                                           args.transparentThreshold,
                                           true);
                    std::lock_guard<std::mutex> lock(mutex);
                    (*resultsRef)[j] = std::move(result);
                }
                {
                    std::lock_guard<std::mutex> lock(cntMutex);
                    quantizedFrameCount++;
                    if (quantizedFrameCount % 10 == 0) {
                        GeneralLogger::info("Quantized " + std::to_string(quantizedFrameCount) + " frames out of " +
                                                std::to_string(frameCount),
                                            GeneralLogger::DETAIL);
                    }
                }
            }
        }));
    }

    for (auto& thread : threads) {
        thread.join();
    }
    return resultsRef;
}

static inline PixelBGRA
genFirstPixel(const u32 lsbLevel) {
    return makeBGRA(0b00000000u | lsbLevel,  // B
                    0b00000011u,             // G
                    0b00000000u,             // r
                    0b11111111u);            // a
}

// fill the palette to fit size (1 << minCodeLength)
// with color of the first pixel at position (end - 2)
// and transparent color at position (end - 1)
static void
fillPalette(std::vector<PixelBGRA>& palette, const u32 lsbLevel, const u32 minCodeLength) {
    const u32 scale    = 1 << (3 * lsbLevel);
    const u32 origSize = palette.size();
    palette.resize(1u << minCodeLength, PixelBGRA{0, 0, 0, 0xffu});
    const u8 mask = TOU8(~((1u << lsbLevel) - 1u));  // e.g. 0b11111100
    for (u32 idx = 0; idx < origSize; ++idx) {
        const auto& p = palette[idx];
        palette[idx]  = makeBGRA(p.b & mask, p.g & mask, p.r & mask, p.a);
    }
    const u8 maskRev = ~mask;  // e.g. 0b00000011
    u32 idx          = origSize;
    for (u32 code = 1; code < scale; ++code) {
        for (u32 offset = 0; offset < origSize; ++offset, ++idx) {
            const auto& p = palette[offset];
            palette[idx]  = makeBGRA((p.b | TOU8(code & maskRev)),
                                    (p.g | TOU8((code >> lsbLevel) & maskRev)),
                                    (p.r | TOU8((code >> (lsbLevel << 1)) & maskRev)),
                                    0xffu);
        }
    }
    *(palette.end() - 2) = genFirstPixel(lsbLevel);
    if (const auto transparentColor = findUnusedColor({palette.data(), palette.size() - 1})) {
        *prev(palette.end()) = *transparentColor;
    } else {
        *prev(palette.end()) = *findUnusedColor({palette.data(), palette.size() - 1}, 1);
    }
}

// return empty vector if quantization for this frame failed
using GetPaletteFunc = std::function<const vector<PixelBGRA>*(u32 frameIndex)>;
using GetIndicesFunc = std::function<const vector<u8>*(u32 frameIndex)>;

static bool
genPalettes(GetPaletteFunc& getPalette,
            GetIndicesFunc& getIndices,
            const EncodeOptions& args,
            const ImageSequenceRef& image,
            const u32 lsbLevel,
            const u32 minCodeLength,
            const u32 frameCount,
            const u32 width,
            const u32 height) {
    DitherMode ditherMode = getDitherMode(args.disableDither, frameCount, args.grayscale);
    if (args.enableLocalPalette) {
        auto quantResultsRef = quant(image, args, args.numColors, ditherMode, frameCount, width, height);
        for (auto& res : *quantResultsRef) {
            if (res.isValid) {
                fillPalette(res.palette, lsbLevel, minCodeLength);
            } else {
                GeneralLogger::error("Error quantifying frame " + std::to_string(&res - quantResultsRef->data()) +
                                     ": " + res.errorMessage);
            }
        }
        getPalette = [quantResultsRef](u32 frameIndex) -> const vector<PixelBGRA>* {
            auto& res = (*quantResultsRef)[frameIndex];
            return res.isValid ? &res.palette : nullptr;
        };
        getIndices = [quantResultsRef](u32 frameIndex) -> const vector<u8>* {
            auto& res = (*quantResultsRef)[frameIndex];
            return res.isValid ? &res.indices : nullptr;
        };
    } else {
        // generate local palettes
        auto quantResultsRef = quant(image, args, 255, ditherMode, frameCount, width, height);
        // generate global palette
        GeneralLogger::info("Generating global palette...", GeneralLogger::STEP);
        vector<PixelBGRA> combined;
        combined.reserve(frameCount * 255);
        for (const auto& res : (*quantResultsRef)) {
            if (!res.isValid) {
                GeneralLogger::error("Error quantifying frame " + std::to_string(&res - quantResultsRef->data()) +
                                     ": " + res.errorMessage);
                continue;
            }
            if (res.palette.size() != 255) {
                GeneralLogger::error("Unexpected palette size for frame " +
                                     std::to_string(&res - quantResultsRef->data()) + ": " +
                                     std::to_string(res.palette.size()));
                continue;
            }
            combined.insert(combined.end(), res.palette.begin(), res.palette.end());
        }
        if (combined.size() == 0) {
            GeneralLogger::error("No valid local palettes were generated.");
            return false;
        }
        auto globalResult =
            quantize(combined, 255, combined.size() / 255, args.numColors, DitherNone, args.grayscale, false, 0, false);
        if (!globalResult.isValid) {
            GeneralLogger::error("Error generating global palette: " + globalResult.errorMessage);
            return false;
        }
        fillPalette(globalResult.palette, lsbLevel, minCodeLength);
        const auto globalPaletteRef = std::make_shared<vector<PixelBGRA>>(std::move(globalResult.palette));
        getPalette = [globalPaletteRef](u32 _) -> const vector<PixelBGRA>* { return globalPaletteRef.get(); };
        // transform local palettes to global palette indices
        u32 combinedIdx = 0;
        for (u32 fIdx = 0; fIdx < frameCount; ++fIdx) {
            if ((*quantResultsRef)[fIdx].palette.size() != 255) {
                continue;
            }
            auto& localIndices = (*quantResultsRef)[fIdx].indices;
            for (auto& index : localIndices) {
                if (index == 255) {
                    index = args.numColors;  // transparent color
                } else {
                    index = globalResult.indices[combinedIdx + index];
                }
            }
            combinedIdx += 255;
        }
        getIndices = [quantResultsRef](u32 frameIndex) -> const vector<u8>* {
            auto& res = (*quantResultsRef)[frameIndex];
            return res.palette.size() != 255 ? nullptr : &res.indices;
        };
    }
    GeneralLogger::info("Quantization completed successfully.", GeneralLogger::STEP);
    return true;
}

class FileReaderException final : public std::exception {
   public:
    explicit FileReaderException(const string&& message) : m_message(message) {}
    [[nodiscard]] const char*
    what() const noexcept override {
        return m_message.c_str();
    }

   private:
    string m_message;
};

class FileReader {
   public:
    FileReader(const string& filePath, const u32 lsbLevel) : m_bitsPerPixel(lsbLevel * 3), m_buffer(READ_CHUNK_SIZE) {
        m_fileName = getFileName(filePath);
        m_filePath = std::filesystem::path(localizePath(filePath));
        if (!std::filesystem::exists(m_filePath)) {
            throw FileReaderException("Input file does not exist: " + filePath);
        }
        m_file = std::ifstream(m_filePath, std::ios::binary);
        if (!m_file.is_open()) {
            throw FileReaderException("Failed to open input file: " + filePath);
        }
        m_file.seekg(0, std::ios::end);
        if (m_file.fail()) {
            throw FileReaderException("Failed to seek to end of file: " + filePath);
        }
        const auto fileSize = m_file.tellg();
        m_file.seekg(0, std::ios::beg);
        if (m_file.fail()) {
            throw FileReaderException("Failed to seek to beginning of file: " + filePath);
        }
        if (fileSize == -1) {
            throw FileReaderException("Failed to get file size: " + filePath);
        }
        m_fileSize = static_cast<size_t>(fileSize);
        setHeader();
    }

    ~FileReader() {
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    [[nodiscard]] u32
    popBits() {
        while (m_byteBufferSize < m_bitsPerPixel) {
            if (m_bufferPos >= m_bufferSize) {
                m_chunksRead += m_bufferSize;
                if (!loadChunk()) break;
            }
            m_byteBuffer <<= 8;
            m_byteBuffer |= m_buffer[m_bufferPos++];
            m_byteBufferSize += 8;
        }
        if (m_byteBufferSize < m_bitsPerPixel) {
            u32 result       = m_byteBuffer << (m_bitsPerPixel - m_byteBufferSize);
            m_byteBufferSize = 0;
            m_byteBuffer     = 0;
            return result;
        }
        m_byteBufferSize -= m_bitsPerPixel;
        u32 result = m_byteBuffer >> m_byteBufferSize;
        m_byteBuffer &= (1u << m_byteBufferSize) - 1u;
        return result;
    }

    [[nodiscard]] inline size_t
    getSize() const {
        return m_fileSize;
    }

    [[nodiscard]] inline size_t
    getBytesRead() const {
        return m_chunksRead + m_bufferPos;
    }

    [[nodiscard]] inline double
    getProgress() const {
        const double ret = static_cast<double>(getBytesRead()) / static_cast<double>(m_fileSize + m_headerSize);
        return ret > 1.0 ? 1.0 : ret;
    }

    [[nodiscard]] inline bool
    isEOF() const {
        return m_file.eof() && m_bufferPos >= m_bufferSize && m_byteBufferSize == 0;
    }

   private:
    void
    setHeader() {
        vector<u8> header;
        const string fileSize = std::to_string(m_fileSize);
        header.insert(header.end(), fileSize.begin(), fileSize.end());
        header.push_back(1);
        // const string extName = getExtName(m_filePath.string());
        // header.insert(header.end(), ENCODED_FILE_NAME.begin(), ENCODED_FILE_NAME.end());
        // header.push_back('.');
        // header.insert(header.end(), extName.begin(), extName.end());
        header.insert(header.end(), m_fileName.begin(), m_fileName.end());
        header.push_back(1);
        if (header.size() >= READ_CHUNK_SIZE) {
            header.resize(READ_CHUNK_SIZE);
            *(header.end() - 2) = 1;
            *(header.end() - 1) = 0;
        }
        // const auto mimeP = MimeTypes::getType(extName.c_str());
        const auto mimeP = MimeTypes::getType(getExtName(m_filePath.string()).c_str());
        string mimeType  = mimeP ? string(mimeP) : "application/octet-stream";
        header.insert(header.end(), mimeType.begin(), mimeType.end());
        header.push_back(0);
        if (header.size() > READ_CHUNK_SIZE) {
            header.resize(READ_CHUNK_SIZE);
            *(header.end() - 1) = 0;
        }
        std::memcpy(m_buffer.data(), header.data(), header.size());
        m_bufferSize = header.size();
        m_headerSize = header.size();
        m_bufferPos  = 0;
        m_isHeader   = true;

        GeneralLogger::info("File name: " + m_fileName, GeneralLogger::STEP);
        GeneralLogger::info("Mime type: " + mimeType, GeneralLogger::STEP);
    }

    inline bool
    loadChunk() {
        if (m_file.eof()) {
            m_bufferPos = m_bufferSize = 0;
            return false;
        }
        m_isHeader = false;
        m_file.read(reinterpret_cast<char*>(m_buffer.data()), READ_CHUNK_SIZE);
        if (m_file.fail() && !m_file.eof()) {
            throw FileReaderException("Fatal error while reading file: " + m_filePath.string());
        }
        m_bufferSize = static_cast<size_t>(m_file.gcount());
        m_bufferPos  = 0;
        return m_bufferSize > 0;
    }

    u32 m_bitsPerPixel = 0;

    std::filesystem::path m_filePath;
    string m_fileName;
    std::ifstream m_file;
    size_t m_fileSize   = 0;
    size_t m_headerSize = 0;
    size_t m_chunksRead = 0;

    vector<u8> m_buffer;
    size_t m_bufferSize = 0;
    size_t m_bufferPos  = 0;

    u32 m_byteBuffer     = 0;
    u32 m_byteBufferSize = 0;

    bool m_isHeader = true;
};

bool
GIFLsb::gifLsbEncode(const EncodeOptions& args) noexcept {
    GeneralLogger::info("Starting GIF LSB encoding...");
    GeneralLogger::info("Output file: " + args.outputFile, GeneralLogger::STEP);
    GeneralLogger::info("Number of colors: " + std::to_string(args.numColors), GeneralLogger::STEP);
    GeneralLogger::info("Disable dither: " + std::to_string(args.disableDither), GeneralLogger::STEP);
    GeneralLogger::info("Transparency: " + std::to_string(args.transparency), GeneralLogger::STEP);
    GeneralLogger::info("Enable local palettes: " + std::to_string(args.enableLocalPalette), GeneralLogger::STEP);
    GeneralLogger::info("Grayscale: " + std::to_string(args.grayscale), GeneralLogger::STEP);
    GeneralLogger::info("Mark text: " + args.markText, GeneralLogger::STEP);
    if (args.transparency) {
        GeneralLogger::info("Transparent threshold: " + std::to_string(args.transparentThreshold), GeneralLogger::STEP);
    }

    GeneralLogger::info("Reading image...");
    auto image = ImageSequence::read(args.imageFile);
    if (!image) {
        return false;
    }
    const u32 frameCount                 = image->getFrameCount();
    const auto delays                    = image->getDelays();
    const u32 width                      = image->getWidth();
    const u32 height                     = image->getHeight();
    const auto [lsbLevel, minCodeLength] = getLsbLevelAndMinCodeLength(args.numColors);

    GeneralLogger::info("Quantifying image...");

    GetPaletteFunc getPalette = nullptr;
    GetIndicesFunc getIndices = nullptr;

    if (!genPalettes(getPalette, getIndices, args, image, lsbLevel, minCodeLength, frameCount, width, height)) {
        return false;
    }

    try {
        GeneralLogger::info("Reading encrypt file...");
        FileReader fileReader(args.encyptFile, lsbLevel);
        const size_t fileSize = fileReader.getSize();
        GeneralLogger::info("Size of file to encrypt: " + std::to_string(fileSize), GeneralLogger::STEP);

        GeneralLogger::info("Initializing GIF encoder...");
        GIFEncoder encoder(args.outputFile,
                           width,
                           height,
                           (1 << minCodeLength) - 1,
                           minCodeLength,
                           args.transparency,
                           (1 << minCodeLength) - 1,
                           0,
                           !args.enableLocalPalette,
                           args.enableLocalPalette ? vector<PixelBGRA>{} : *getPalette(0));

        GeneralLogger::info("Generating frames...");
        u32 frameIndex      = 0;
        u32 generatedFrames = 0;
        bool isFirstPixel   = true;
        vector<u8> frameResultBuffer(width * height);
        double lastProgress = 0.0;
        while (true) {
            const auto indices = getIndices(frameIndex);
            if (const auto palette = getPalette(frameIndex); indices && palette) {
                auto bufferIt = frameResultBuffer.begin();
                for (const auto index : *indices) {
                    u8 res;
                    if (isFirstPixel) {
                        res          = (1 << minCodeLength) - 2;
                        isFirstPixel = false;
                    } else if (index == args.numColors) {  // transparent color
                        res = (1 << minCodeLength) - 1;
                    } else {
                        const auto bits = fileReader.popBits();
                        res             = args.numColors * bits + index;
                        // const auto& color       = (*palette)[res];
                        // const u32 bitsFromColor = (color.b & 1) | ((color.g & 1) << 1) | ((color.r & 1) << 2);
                        // if (bits != bitsFromColor) {
                        //     GeneralLogger::error("Error: bits mismatch: " + std::to_string(bits) +
                        //                          " != " + std::to_string(bitsFromColor));
                        //     return false;
                        // }
                    }
                    *bufferIt++ = res;

                    // static FILE* tmp = fopen("tmp-enc.txt", "w");
                    // fprintf(tmp,
                    //         "%02hhx%02hhx%02hhx%02hhx\n",
                    //         palette->at(res).b,
                    //         palette->at(res).g,
                    //         palette->at(res).r,
                    //         palette->at(res).a);
                }
                encoder.addFrame({frameResultBuffer.data(), frameResultBuffer.size()},
                                 delays[frameIndex],
                                 args.transparency ? 3 : 1,
                                 minCodeLength,
                                 args.enableLocalPalette ? *palette : vector<PixelBGRA>{});
            }
            frameIndex++;
            generatedFrames++;
            const double progress = fileReader.getProgress();
            if (progress - lastProgress >= PROGRESS_STEP) {
                char progressBuffer[64];
                snprintf(progressBuffer, sizeof(progressBuffer), "Progress: %.2f%%", progress * 100);
                GeneralLogger::info(progressBuffer, GeneralLogger::STEP);
                lastProgress = progress;
            }
            if (frameIndex >= frameCount) {
                if (!fileReader.getBytesRead()) {
                    throw GIFEncodeException("All pixels are transparent or unavailable.");
                }
                if (fileReader.isEOF()) {
                    break;
                }
                frameIndex = 0;
            }
        }

        if (!encoder.finish()) {
            throw GIFEncodeException("Unknown error.");
        }
        if (lastProgress < 1.0) {
            GeneralLogger::info("Progress: 100.00%", GeneralLogger::STEP);
        }
        GeneralLogger::info("Encoding completed successfully.");
        GeneralLogger::info("Generated frames: " + std::to_string(generatedFrames));
        GeneralLogger::info("Output file: " + encoder.getFileName());
        return true;
    } catch (const GIFEncodeException& e) {
        GeneralLogger::error("Failed encoding GIF: " + string(e.what()));
    } catch (const FileReaderException& e) {
        GeneralLogger::error("Failed reading file: " + string(e.what()));
    } catch (const std::exception& e) {
        GeneralLogger::error("Unexpected error: " + string(e.what()));
    } catch (...) {
        GeneralLogger::error("Unknown error occurred.");
    }
    return false;
}