#include <functional>
#include <string>
#include <vector>

#include "file_utils.h"
#include "gif_lsb.h"
#include "imsq_stream.h"
#include "log.h"

using namespace GIFImage;
using namespace GIFLsb;
using std::string, std::vector;

static constexpr uint32_t WRITE_BUFFER_SIZE = 1 << 20;  // 1 MiB
static constexpr double PROGRESS_STEP       = 0.0314;   // 3.14%

using PopByteFunc = std::function<uint8_t()>;

struct HeaderData {
    const size_t fileSize;
    const string fileName;
    const string mimeType;
};

class EOFException final : public std::exception {};

class DecodingException final : public std::exception {
  public:
    explicit DecodingException(const string&& message)
        : msg(message) {}

    [[nodiscard]] const char*
    what() const noexcept override {
        return msg.c_str();
    }

  private:
    string msg;
};

static uint32_t
getLsbLevel(const PixelBGRA& pixel) {
    if ((pixel.r & 7) != 0 || (pixel.g & 7) != 3) return 0;
    return pixel.b & 7;
}

static uint32_t
toBits(const PixelBGRA& pixel, const uint32_t lsbLevel, const uint32_t mask) {
    return ((static_cast<uint32_t>(pixel.b) & mask) | ((static_cast<uint32_t>(pixel.g) & mask) << lsbLevel) |
            ((static_cast<uint32_t>(pixel.r) & mask) << (2 * lsbLevel)));
}

static HeaderData
decodeHeader(const PopByteFunc& func) {
    size_t fileSize = 0;
    uint8_t byte;
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
    GeneralLogger::info("Input file: " + args.imagePath, GeneralLogger::STEP);

    const auto& image = args.image;
    if (!image) {
        GeneralLogger::error("Failed to read image: " + args.imagePath);
        return false;
    }

    try {
        auto frame              = image->getNextFrame();
        uint32_t byteBuffer     = 0;
        uint32_t byteBufferSize = 0;
        while (!frame) {
            if (image->isEndOfStream()) {
                GeneralLogger::error("No valid frames found in image.");
                return false;
            }
            frame = image->getNextFrame();
        }
        auto pixelItr = frame->buffer.begin();

        GeneralLogger::info("Parsing header...");
        const uint32_t lsbLevel = getLsbLevel(*pixelItr++);
        if (lsbLevel == 0 || lsbLevel > 7) {
            GeneralLogger::error("Invalid LSB encryption format");
            return false;
        }
        GeneralLogger::info("LSB level: " + std::to_string(lsbLevel), GeneralLogger::STEP);
        const uint32_t mask = (1u << lsbLevel) - 1u;

        const PopByteFunc popByte =
            [&image, &pixelItr, &frame, &byteBuffer, &byteBufferSize, lsbLevel, mask]() -> uint8_t {
            while (byteBufferSize < 8) {
                if (pixelItr == frame->buffer.end()) {
                    do {
                        frame = image->getNextFrame();
                    } while (!frame && !image->isEndOfStream());
                    if (!frame) {
                        throw EOFException();
                    }
                    pixelItr = frame->buffer.begin();
                }
                // skip transparent pixels
                // (incompatible with traditional LSB)
                if (pixelItr->a == 0) {
                    ++pixelItr;
                    continue;
                }
                byteBuffer <<= lsbLevel * 3;
                byteBufferSize += lsbLevel * 3;
                byteBuffer |= toBits(*pixelItr++, lsbLevel, mask);
            }
            byteBufferSize -= 8;
            uint32_t ret = byteBuffer & (0xffu << byteBufferSize);
            byteBuffer   = byteBuffer & ((1 << byteBufferSize) - 1);
            return ret >> byteBufferSize;
        };

        const auto headerData = decodeHeader(popByte);
        GeneralLogger::info("File size: " + std::to_string(headerData.fileSize), GeneralLogger::STEP);
        GeneralLogger::info("File name: " + headerData.fileName, GeneralLogger::STEP);
        GeneralLogger::info("MIME type: " + headerData.mimeType, GeneralLogger::STEP);

        GeneralLogger::info("Decoding frames...");
        // std::filesystem::path outputPath;
        string fileName;
        if (!args.outputName.empty()) {
            fileName = NaiveIO::replaceExtName(args.outputName, NaiveIO::getExtName(headerData.fileName));
        } else if (!headerData.fileName.empty() && NaiveIO::isValidFileName(headerData.fileName)) {
            fileName = headerData.fileName;
        } else {
            fileName = "decrypted_" +
                       std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                       NaiveIO::getExtName(headerData.fileName);
        }

        vector<uint8_t> buffer(WRITE_BUFFER_SIZE);
        size_t bufferPos    = 0;
        size_t wroteSize    = 0;
        double lastProgress = 0.0;
        for (size_t i = 0; i < headerData.fileSize; ++i) {
            const uint8_t byte  = popByte();
            buffer[bufferPos++] = byte;
            if (bufferPos >= WRITE_BUFFER_SIZE) {
                args.outputFile->write(buffer);
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
            args.outputFile->write({buffer.data(), bufferPos});
        }
        args.outputFile->close();
        if (lastProgress < 1.0) {
            GeneralLogger::info("Progress: 100.00%", GeneralLogger::DETAIL);
        }
        if (!args.outputFile->rename(fileName)) {
            throw DecodingException("Failed to rename output file.");
        }
        GeneralLogger::info("Decoding completed successfully.");
        GeneralLogger::info("Output file: " + args.outputFile->getFilePath());
        return true;
    } catch (const EOFException&) {
        GeneralLogger::error("End of file reached before decoding completed.");
    } catch (const DecodingException& e) {
        GeneralLogger::error("Decoding error: " + string(e.what()));
    } catch (const std::ios_base::failure& e) {
        GeneralLogger::error("I/O error: " + string(e.what()));
    } catch (const std::exception& e) {
        GeneralLogger::error("Unexpected error: " + string(e.what()));
    }
    args.outputFile->close();
    args.outputFile->deleteFile();
    return false;
}