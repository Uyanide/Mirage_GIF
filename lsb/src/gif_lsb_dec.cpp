
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "gif_lsb.h"
#include "imsq.h"
#include "log.h"
#include "path.h"

using namespace GIFImage;
using namespace GIFLsb;
using std::string, std::vector;

static constexpr u32 WRITE_BUFFER_SIZE = 1 << 18;  // 256 KiB
static constexpr double PROGRESS_STEP  = 0.0314;   // 3.14%

using PopByteFunc = std::function<u8()>;

struct HeaderData {
    const size_t fileSize;
    const string fileName;
    const string mimeType;
};

class EOFException final : public std::exception {};

class DecodingException final : public std::exception {
   public:
    explicit DecodingException(const string&& message) : msg(message) {}
    [[nodiscard]] const char*
    what() const noexcept override {
        return msg.c_str();
    }

   private:
    string msg;
};

static u32
getLsbLevel(const PixelBGRA& pixel) {
    if ((pixel.r & 7) != 0 || (pixel.g & 7) != 3) return 0;
    return pixel.b & 7;
}

static inline u32
toBits(const PixelBGRA& pixel, const u32 lsbLevel, const u32 mask) {
    return ((static_cast<u32>(pixel.b) & mask) | ((static_cast<u32>(pixel.g) & mask) << lsbLevel) |
            ((static_cast<u32>(pixel.r) & mask) << (2 * lsbLevel)));
}

static HeaderData
decodeHeader(const PopByteFunc& func) {
    size_t fileSize = 0;
    u8 byte;
    while (true) {
        byte = func();
        if (byte == 1 || byte == 0) break;
        if (byte < '0' || byte > '9') {
            throw DecodingException("Invalid file size.");
        }
        fileSize = fileSize * 10 + byte - '0';
    }
    if (byte == 0) {
        return {fileSize, "", ""};
    }
    string fileName;
    while (true) {
        byte = func();
        if (byte == 1 || byte == 0) break;
        fileName.push_back(static_cast<char>(byte));
    }
    if (byte == 0) {
        return {fileSize, fileName, ""};
    }
    string mimeType;
    while (true) {
        byte = func();
        if (byte == 0) break;
        mimeType.push_back(static_cast<char>(byte));
    }
    return {fileSize, fileName, mimeType};
}

bool
GIFLsb::gifLsbDecode(const DecodeOptions& args) noexcept {
    GeneralLogger::info("Starting LSB decoding...");
    GeneralLogger::info("Input file: " + args.decyptImage, GeneralLogger::STEP);

    const auto image = ImageSequence::read(args.decyptImage);
    if (!image) {
        GeneralLogger::error("Failed to read image: " + args.decyptImage);
        return false;
    }

    try {
        u32 frameCount          = image->getFrameCount();
        u32 frameIndex          = 0;
        vector<PixelBGRA> frame = image->getFrameBuffer(0, 0, 0, true);
        u32 byteBuffer          = 0;
        u32 byteBufferSize      = 0;
        while (frame.empty()) {
            if (++frameIndex >= frameCount) {
                GeneralLogger::error("No valid frames found in image.");
                return false;
            }
            frame = image->getFrameBuffer(frameIndex, 0, 0, true);
        }
        auto pixelItr = frame.begin();

        GeneralLogger::info("Parsing header...");
        const u32 lsbLevel = getLsbLevel(*pixelItr++);
        if (lsbLevel == 0 || lsbLevel > 7) {
            GeneralLogger::error("Invalid LSB encryption format");
            return false;
        }
        GeneralLogger::info("LSB level: " + std::to_string(lsbLevel), GeneralLogger::STEP);
        const u32 mask = (1u << lsbLevel) - 1u;

        const PopByteFunc popByte =
            [&image, &pixelItr, &frame, &frameIndex, frameCount, &byteBuffer, &byteBufferSize, lsbLevel, mask]() -> u8 {
            while (byteBufferSize < 8) {
                while (pixelItr == frame.end()) {
                    if (++frameIndex >= frameCount) {
                        throw EOFException();
                    }
                    frame    = image->getFrameBuffer(frameIndex, 0, 0, true);
                    pixelItr = frame.begin();
                }
                if (pixelItr->a == 0) {
                    ++pixelItr;
                    continue;
                }
                byteBuffer <<= lsbLevel * 3;
                byteBufferSize += lsbLevel * 3;

                // static FILE* tmp = fopen("tmp-dec.txt", "w");
                // fprintf(tmp, "%02hhx%02hhx%02hhx%02hhx\n", pixelItr->b, pixelItr->g, pixelItr->r, pixelItr->a);

                byteBuffer |= toBits(*pixelItr++, lsbLevel, mask);
            }
            byteBufferSize -= 8;
            u32 ret    = byteBuffer & (0xffu << byteBufferSize);
            byteBuffer = byteBuffer & ((1 << byteBufferSize) - 1);
            return ret >> byteBufferSize;
        };

        const auto headerData = decodeHeader(popByte);
        GeneralLogger::info("File size: " + std::to_string(headerData.fileSize), GeneralLogger::STEP);
        GeneralLogger::info("File name: " + headerData.fileName, GeneralLogger::STEP);
        GeneralLogger::info("MIME type: " + headerData.mimeType, GeneralLogger::STEP);

        GeneralLogger::info("Decoding frames...");
        std::ofstream outFile;
        std::filesystem::path outputPath;
        if (!headerData.fileName.empty() && args.outputFile.empty()) {
            outputPath = std::filesystem::path(localizePath(args.outputDirectory + headerData.fileName));
        } else if (!args.outputFile.empty()) {
            outputPath = std::filesystem::path(localizePath(args.outputDirectory + args.outputFile))
                             .replace_extension(getExtName(headerData.fileName));
        } else {
            outputPath = std::filesystem::path(
                localizePath(args.outputDirectory + "decrypted" + getExtName(headerData.fileName)));
        }
        outFile.open(outputPath, std::ios::binary);
        if (!outFile.is_open()) {
            GeneralLogger::error("Failed to open output file: " + outputPath.string());
            return false;
        }
        outFile.exceptions(std::ios::failbit | std::ios::badbit);

        vector<u8> buffer(WRITE_BUFFER_SIZE);
        size_t bufferPos    = 0;
        size_t wroteSize    = 0;
        double lastProgress = 0.0;
        for (size_t i = 0; i < headerData.fileSize; ++i) {
            const u8 byte       = popByte();
            buffer[bufferPos++] = byte;
            if (bufferPos >= WRITE_BUFFER_SIZE) {
                outFile.write(reinterpret_cast<const char*>(buffer.data()), bufferPos);
                wroteSize += bufferPos;
                bufferPos             = 0;
                const double progress = static_cast<double>(wroteSize) / headerData.fileSize;
                if (progress - lastProgress >= PROGRESS_STEP) {
                    char progressBuffer[64];
                    snprintf(progressBuffer, sizeof(progressBuffer), "Progress: %.2f%%", progress * 100.0);
                    GeneralLogger::info(progressBuffer, GeneralLogger::DETAIL);
                    lastProgress = progress;
                }
            }
        }
        if (bufferPos > 0) {
            outFile.write(reinterpret_cast<const char*>(buffer.data()), bufferPos);
        }
        outFile.close();
        if (lastProgress < 1.0) {
            GeneralLogger::info("Progress: 100.00%", GeneralLogger::DETAIL);
        }
        GeneralLogger::info("Decoding completed successfully.");
        GeneralLogger::info("Output file: " + deLocalizePath(outputPath.string()));
        return true;
    } catch (const EOFException&) {
        GeneralLogger::error("End of file reached before decoding completed.");
        return false;
    } catch (const DecodingException& e) {
        GeneralLogger::error("Decoding error: " + string(e.what()));
        return false;
    } catch (const std::ios_base::failure& e) {
        GeneralLogger::error("I/O error: " + string(e.what()));
        return false;
    } catch (const std::exception& e) {
        GeneralLogger::error("Unexpected error: " + string(e.what()));
        return false;
    }
}