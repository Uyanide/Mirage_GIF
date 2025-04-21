#include <cstddef>
#include <cstring>
#include <functional>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

#include "MimeTypes.h"
#include "file_reader.h"
#include "file_utils.h"
#include "gif_encoder.h"
#include "gif_exception.h"
#include "gif_lsb.h"
#include "imsq.h"
#include "log.h"
#include "mark.h"
#include "quantizer.h"

using std::string, std::vector;
using namespace GIFImage;
using namespace GIFLsb;
using namespace GIFEnc;

static constexpr size_t MAX_HEADER_SIZE = 1 << 12;  // 4 KiB
static constexpr size_t READ_CHUNK_SIZE = 1 << 20;  // 1 MiB
// static const string ENCODED_FILE_NAME   = "mtk";
static constexpr double PROGRESS_STEP = 0.0314;  // 3.14%

// (1 << (3 * lsbLevel)) * numColors + 2 < 1 << minCodeLength
//                                     ^ these 2: header pixel & transparent color (if any)
// info-density: 3 * lsbLevel / minCodeLength
static std::pair<uint32_t, uint32_t>
getLsbLevelAndMinCodeLength(const uint32_t numColors) {
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
getDitherMode(const bool disableDither, const uint32_t frameCount, const bool grayscale) {
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
quant(const ImageSequence::Ref& image,
      const EncodeOptions& args,
      const vector<PixelBGRA>& markImage,
      const uint32_t markWidth,
      const uint32_t markHeight,
      const uint32_t numColors,
      const DitherMode ditherMode,
      const uint32_t frameCount,
      const uint32_t width,
      const uint32_t height) {
    uint32_t quantizedFrameCount = 0;
    std::mutex cntMutex;
    auto resultsRef = std::make_shared<vector<QuantizerResult>>(frameCount);
    std::mutex mutex;
    vector<std::thread> threads;

    auto threadCount = args.threadCount;
    if (threadCount > frameCount) {
        GeneralLogger::warn("Number of threads is greater than number of frames. Reducing to " +
                            std::to_string(frameCount));
        threadCount = frameCount;
    }
    const uint32_t framePerThread = (frameCount + threadCount - 1) / threadCount;

    for (uint32_t i = 0; i < threadCount; ++i) {
        threads.push_back(std::thread([&, i]() {
            uint32_t startFrame = i * framePerThread;
            uint32_t endFrame   = startFrame + framePerThread;
            if (endFrame > frameCount) endFrame = frameCount;
            for (uint32_t j = startFrame; j < endFrame; ++j) {
                auto frameBuffer = image->getFrameBuffer(j, width, height);
                if (!markImage.empty()) {
                    ImageSequence::drawMark(frameBuffer, width, height, markImage, markWidth, markHeight, 0, 0);
                } else if (args.markText != "none") {
                    ImageSequence::drawText(frameBuffer, width, height, args.markText);
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

static PixelBGRA
genFirstPixel(const uint32_t lsbLevel) {
    return makeBGRA(0b00000000u | lsbLevel,  // B
                    0b00000011u,             // G
                    0b00000000u,             // r
                    0b11111111u);            // a
}

// fill the palette to fit size (1 << minCodeLength)
// with color of the first pixel at position (end - 2)
// and transparent color at position (end - 1)
static void
fillPalette(std::vector<PixelBGRA>& palette, const uint32_t lsbLevel, const uint32_t minCodeLength) {
    const uint32_t scale    = 1 << (3 * lsbLevel);
    const uint32_t origSize = palette.size();
    palette.resize(1u << minCodeLength, PixelBGRA{0, 0, 0, 0xffu});
    const uint8_t mask = TOU8(~((1u << lsbLevel) - 1u));  // e.g. 0b11111100
    for (uint32_t idx = 0; idx < origSize; ++idx) {
        const auto& p = palette[idx];
        palette[idx]  = makeBGRA(p.b & mask, p.g & mask, p.r & mask, p.a);
    }
    const uint8_t maskRev = ~mask;  // e.g. 0b00000011
    uint32_t idx          = origSize;
    for (uint32_t code = 1; code < scale; ++code) {
        for (uint32_t offset = 0; offset < origSize; ++offset, ++idx) {
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
using GetPaletteFunc = std::function<const vector<PixelBGRA>*(uint32_t frameIndex)>;
using GetIndicesFunc = std::function<const vector<uint8_t>*(uint32_t frameIndex)>;

static bool
genPalettes(GetPaletteFunc& getPalette,
            GetIndicesFunc& getIndices,
            const EncodeOptions& args,
            const ImageSequence::Ref& image,
            const vector<PixelBGRA>& markImage,
            const uint32_t markWidth,
            const uint32_t markHeight,
            const uint32_t lsbLevel,
            const uint32_t minCodeLength,
            const uint32_t frameCount,
            const uint32_t width,
            const uint32_t height) {
    DitherMode ditherMode = getDitherMode(args.disableDither, frameCount, args.grayscale);
    if (args.enableLocalPalette) {
        auto quantResultsRef = quant(image, args, markImage, markWidth, markHeight, args.numColors, ditherMode, frameCount, width, height);
        for (auto& res : *quantResultsRef) {
            if (res.isValid) {
                fillPalette(res.palette, lsbLevel, minCodeLength);
            } else {
                GeneralLogger::error("Error quantifying frame " + std::to_string(&res - quantResultsRef->data()) +
                                     ": " + res.errorMessage);
            }
        }
        getPalette = [quantResultsRef](uint32_t frameIndex) -> const vector<PixelBGRA>* {
            auto& res = (*quantResultsRef)[frameIndex];
            return res.isValid ? &res.palette : nullptr;
        };
        getIndices = [quantResultsRef](uint32_t frameIndex) -> const vector<uint8_t>* {
            auto& res = (*quantResultsRef)[frameIndex];
            return res.isValid ? &res.indices : nullptr;
        };
    } else {
        // generate local palettes
        auto quantResultsRef = quant(image, args, markImage, markWidth, markHeight, 255, ditherMode, frameCount, width, height);
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

        getPalette = [globalPaletteRef](uint32_t _) -> const vector<PixelBGRA>* {
            return globalPaletteRef.get();
        };
        // transform local palettes to global palette indices
        uint32_t combinedIdx = 0;
        for (uint32_t fIdx = 0; fIdx < frameCount; ++fIdx) {
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
        getIndices = [quantResultsRef](uint32_t frameIndex) -> const vector<uint8_t>* {
            auto& res = (*quantResultsRef)[frameIndex];
            return res.palette.size() != 255 ? nullptr : &res.indices;
        };
    }
    GeneralLogger::info("Quantization completed successfully.", GeneralLogger::STEP);
    return true;
}

class LsbFileReader {
  public:
    LsbFileReader(const NaiveIO::FileReader::Ref& fileReader, const string& filePath, const uint32_t lsbLevel)
        : m_bitsPerPixel(lsbLevel * 3), m_file(fileReader), m_buffer(READ_CHUNK_SIZE) {
        m_fileName = NaiveIO::getFileName(filePath);
        m_fileSize = m_file->getSize();
        setHeader();
    }

    [[nodiscard]] uint32_t
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
            uint32_t result  = m_byteBuffer << (m_bitsPerPixel - m_byteBufferSize);
            m_byteBufferSize = 0;
            m_byteBuffer     = 0;
            return result;
        }
        m_byteBufferSize -= m_bitsPerPixel;
        uint32_t result = m_byteBuffer >> m_byteBufferSize;
        m_byteBuffer &= (1u << m_byteBufferSize) - 1u;
        return result;
    }

    [[nodiscard]] size_t
    getSize() const {
        return m_fileSize;
    }

    [[nodiscard]] size_t
    getBytesRead() const {
        return m_chunksRead + m_bufferPos;
    }

    [[nodiscard]] double
    getProgress() const {
        const double ret = static_cast<double>(getBytesRead()) / static_cast<double>(m_fileSize + m_headerSize);
        return ret > 1.0 ? 1.0 : ret;
    }

    [[nodiscard]] bool
    isEOF() const {
        return m_file->isEOF() && m_bufferPos >= m_bufferSize && m_byteBufferSize == 0;
    }

  private:
    void
    setHeader() {
        vector<uint8_t> header;
        const string fileSize = std::to_string(m_fileSize);
        header.insert(header.end(), fileSize.begin(), fileSize.end());
        header.push_back(1);
        // const string extName = getExtName(m_filePath.string());
        // header.insert(header.end(), ENCODED_FILE_NAME.begin(), ENCODED_FILE_NAME.end());
        // header.push_back('.');
        // header.insert(header.end(), extName.begin(), extName.end());
        header.insert(header.end(), m_fileName.begin(), m_fileName.end());
        header.push_back(1);
        if (header.size() >= MAX_HEADER_SIZE) {
            header.resize(MAX_HEADER_SIZE);
            *(header.end() - 2) = 1;
            *(header.end() - 1) = 0;
        }
        // const auto mimeP = MimeTypes::getType(extName.c_str());
        const auto mimeP = MimeTypes::getType(NaiveIO::getExtName(m_fileName.c_str()).c_str());
        string mimeType  = mimeP ? string(mimeP) : "application/octet-stream";
        header.insert(header.end(), mimeType.begin(), mimeType.end());
        header.push_back(0);
        if (header.size() > MAX_HEADER_SIZE) {
            header.resize(MAX_HEADER_SIZE);
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

    bool
    loadChunk() {
        if (m_file->isEOF()) {
            m_bufferPos = m_bufferSize = 0;
            return false;
        }
        m_isHeader = false;
        std::span<uint8_t> chunk(m_buffer.data(), m_buffer.size());
        m_bufferSize = m_file->read(chunk);
        m_bufferPos  = 0;
        return m_bufferSize > 0;
    }

    uint32_t m_bitsPerPixel = 0;

    string m_fileName;
    const NaiveIO::FileReader::Ref& m_file;
    size_t m_fileSize   = 0;
    size_t m_headerSize = 0;
    size_t m_chunksRead = 0;

    vector<uint8_t> m_buffer;
    size_t m_bufferSize = 0;
    size_t m_bufferPos  = 0;

    uint32_t m_byteBuffer     = 0;
    uint32_t m_byteBufferSize = 0;

    bool m_isHeader = true;
};

static size_t
getRequiredSize(const uint32_t lsbLevel, const size_t fileDataSize) {
    return (fileDataSize + MAX_HEADER_SIZE) * 8 / lsbLevel / 3;
}

bool
GIFLsb::gifLsbEncode(const EncodeOptions& args) noexcept {
    GeneralLogger::info("Starting GIF LSB encoding...");
    GeneralLogger::info("Output file: " + args.outputFile->getFilePath(), GeneralLogger::STEP);
    GeneralLogger::info("Number of colors: " + std::to_string(args.numColors), GeneralLogger::STEP);
    GeneralLogger::info("Disable dither: " + std::to_string(args.disableDither), GeneralLogger::STEP);
    GeneralLogger::info("Transparency: " + std::to_string(args.transparency), GeneralLogger::STEP);
    GeneralLogger::info("Enable local palettes: " + std::to_string(args.enableLocalPalette), GeneralLogger::STEP);
    GeneralLogger::info("Generate single frame: " + std::to_string(args.singleFrame), GeneralLogger::STEP);
    GeneralLogger::info("Grayscale: " + std::to_string(args.grayscale), GeneralLogger::STEP);
    GeneralLogger::info("Mark text: " + args.markText, GeneralLogger::STEP);
    if (args.transparency) {
        GeneralLogger::info("Transparent threshold: " + std::to_string(args.transparentThreshold), GeneralLogger::STEP);
    }

    vector<PixelBGRA> markImage;
    if (args.markText == markIdentifier) {
        GeneralLogger::info("Loading mark image...");
        markImage = ImageSequence::parseBase64(markBase64);
        if (markImage.size() != GIFLsb::markWidth * GIFLsb::markHeight) {
            GeneralLogger::error("Invalid mark image size: " + std::to_string(markImage.size()));
            markImage.clear();
        }
    }

    GeneralLogger::info("Reading image...");
    auto& image = args.image;
    if (!image) {
        return false;
    }
    uint32_t frameCount                  = image->getFrameCount();
    const vector<uint32_t>& delays       = image->getDelays();
    uint32_t width                       = image->getWidth();
    uint32_t height                      = image->getHeight();
    const auto [lsbLevel, minCodeLength] = getLsbLevelAndMinCodeLength(args.numColors);

    try {

        GeneralLogger::info("Reading encrypt file...");
        LsbFileReader fileReader(args.file, args.filePath, lsbLevel);
        const size_t fileSize = fileReader.getSize();
        GeneralLogger::info("Size of file to encrypt: " + std::to_string(fileSize), GeneralLogger::STEP);

        if (args.singleFrame) {
            const auto requiredSize = getRequiredSize(lsbLevel, fileSize);
            if (requiredSize > width * height) {
                const auto ratio = std::sqrt(static_cast<double>(requiredSize) / (width * height));
                width            = static_cast<uint32_t>(std::ceil(width * ratio));
                height           = static_cast<uint32_t>(std::ceil(height * ratio));
                GeneralLogger::warn("Image does not have enough pixels to store the file. Resized to " +
                                    std::to_string(width) + "x" + std::to_string(height));
            }
            frameCount = 1;
        }

        uint32_t markWidth = 0, markHeight = 0;
        if (!markImage.empty()) {
            markHeight = height * args.markRatio;
            markWidth  = static_cast<double>(markHeight) * GIFLsb::markWidth / GIFLsb::markHeight;

            markImage = ImageSequence::resizeCover(
                markImage,
                GIFLsb::markWidth,
                GIFLsb::markHeight,
                markWidth,
                markHeight);
        }

        GeneralLogger::info("Quantifying image...");

        GetPaletteFunc getPalette = nullptr;
        GetIndicesFunc getIndices = nullptr;

        if (!genPalettes(getPalette, getIndices, args, image, markImage, markWidth, markHeight, lsbLevel, minCodeLength, frameCount, width, height)) {
            return false;
        }

        GeneralLogger::info("Initializing GIF encoder...");
        GIFEncoder encoder(
            [&args](const std::span<const uint8_t> data) -> bool {
                try {
                    if (args.outputFile->write(data) != data.size()) {
                        return false;
                    }
                    return true;
                } catch (std::exception& e) {
                    GeneralLogger::error(std::string("Failed to write GIF file: ") + e.what());
                } catch (...) {
                    GeneralLogger::error("Failed to write GIF file: unknown error");
                }
                return false;
            },
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
        uint32_t frameIndex      = 0;
        uint32_t generatedFrames = 0;
        bool isFirstPixel        = true;
        vector<uint8_t> frameResultBuffer(width * height);
        double lastProgress = 0.0;
        while (true) {
            const auto indices = getIndices(frameIndex);
            if (const auto palette = getPalette(frameIndex); indices && palette) {
                auto bufferIt = frameResultBuffer.begin();
                for (const auto index : *indices) {
                    uint8_t res;
                    if (isFirstPixel) {
                        res          = (1 << minCodeLength) - 2;
                        isFirstPixel = false;
                    } else if (index == args.numColors) {  // transparent color
                        res = (1 << minCodeLength) - 1;
                    } else {
                        const auto bits = fileReader.popBits();
                        res             = args.numColors * bits + index;
                    }
                    *bufferIt++ = res;
                }
                encoder.addFrame(frameResultBuffer,
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
        GeneralLogger::info("Output file: " + args.outputFile->getFilePath());
        return true;
    } catch (const GIFEncodeException& e) {
        GeneralLogger::error("Failed encoding GIF: " + string(e.what()));
    } catch (const NaiveIO::FileReaderException& e) {
        GeneralLogger::error("Failed reading file: " + string(e.what()));
    } catch (const std::exception& e) {
        GeneralLogger::error("Unexpected error: " + string(e.what()));
    } catch (...) {
        GeneralLogger::error("Unknown error occurred.");
    }
    return false;
}