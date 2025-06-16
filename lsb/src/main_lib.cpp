#include <cstring>
#include <ostream>
#include <sstream>
#include <vector>

#include "def.h"
#include "file_reader.h"
#include "gif_lsb.h"
#include "imsq_stream.h"
#include "interface.h"
#include "log.h"
#include "options.h"

static std::stringstream errorStream{};
std::ostream* GeneralLogger::logStream = &errorStream;

EXTERN_C void EXPORT
gifLsbEncode(
    uint8_t** frames,
    uint32_t* delays,
    uint32_t frameCount,
    uint32_t width,
    uint32_t height,
    uint8_t* data,
    uint32_t dataSize,
    char* markText,
    char* fileName,
    char* outputFilePath,
    char* errorMessage,
    uint32_t errorMessageSize,
    uint32_t colors,
    int32_t grayScale,
    int32_t transparency,
    int32_t transparencyThreshold,
    int32_t localPalette,
    int32_t single) {
    GIFLsb::EncodeOptions options;
    options.file = NaiveIO::FileReader::createFromMemory(
        {data,
         dataSize},
        fileName);
    options.outputFile = NaiveIO::FileWriter::create(outputFilePath);
    std::vector<std::vector<PixelBGRA>> framesVec;
    for (uint32_t i = 0; i < frameCount; ++i) {
        std::vector<PixelBGRA> frame(width * height);
        for (uint32_t j = 0; j < width * height * 4; j += 4) {
            frame[j / 4] = makeBGRA(frames[i][j + 2], frames[i][j + 1], frames[i][j], frames[i][j + 3]);
        }
        framesVec.push_back(std::move(frame));
    }
    options.image                = GIFImage::ImageSequence::load(framesVec, {delays, frameCount}, width, height);
    options.imagePath            = "image";
    options.filePath             = fileName;
    options.markText             = markText;
    options.disableDither        = false;
    options.transparency         = transparency != 0;
    options.grayscale            = grayScale != 0;
    options.enableLocalPalette   = localPalette != 0;
    options.singleFrame          = single != 0;
    options.outputPath           = outputFilePath;
    options.numColors            = colors;
    options.transparentThreshold = transparencyThreshold;
    options.threadCount          = 0;  // Auto-detect

    const auto ret = options.ensureValid();
    if (ret) {
        std::strncpy(errorMessage, ret->c_str(), errorMessageSize - 1);
        return;
    }

    const auto precessRet = GIFLsb::gifLsbEncode(options);
    if (!precessRet) {
        const auto errorMsg = errorStream.str();
        std::strncpy(errorMessage, errorMsg.c_str(), errorMessageSize - 1);
        errorMessage[errorMessageSize - 1] = '\0';  // Ensure null-termination
        return;
    }
}

EXTERN_C void EXPORT
gifLsbDecode(
    uint8_t** frames,
    uint32_t frameCount,
    uint32_t* width,
    uint32_t* height,
    char* outputFilePath) {
    GIFLsb::DecodeOptions options;

    std::vector<std::vector<PixelBGRA>> framesVec;
    for (uint32_t i = 0; i < frameCount; ++i) {
        std::vector<PixelBGRA> frame(*width * *height);
        for (uint32_t j = 0; j < (*width) * (*height) * 4; j += 4) {
            frame[j / 4] = makeBGRA(frames[i][j + 2], frames[i][j + 1], frames[i][j], frames[i][j + 3]);
        }
        framesVec.push_back(std::move(frame));
    }
    options.image      = GIFImage::ImageSequenceStream::load(framesVec, {}, {width, frameCount}, {height, frameCount});
    options.imagePath  = "image";
    options.outputFile = NaiveIO::FileWriter::create(outputFilePath);

    const auto ret = options.ensureValid();
    if (ret) {
        std::strncpy(outputFilePath, ret->c_str(), 255);
        return;
    }

    const auto precessRet = GIFLsb::gifLsbDecode(options);
    if (!precessRet) {
        std::strncpy(outputFilePath, "Decoding failed", 255);
        return;
    }
}

std::optional<std::string>
GIFLsb::DecodeOptions::ensureValid() {
    if (!image) {
        return "Invalid image file.";
    }
    if (!outputFile) {
        return "Invalid output filename or directory.";
    }
    return std::nullopt;
}

std::optional<std::string>
GIFLsb::EncodeOptions::ensureValid() {
    if (!image) {
        return "Invalid image file.";
    }
    if (!file) {
        return "Invalid file to encrypt.";
    }
    if (!outputFile) {
        return "Invalid output file.";
    }
    if (numColors < Limits::MIN_NUM_COLORS || numColors > Limits::MAX_NUM_COLORS) {
        return "Number of colors must be between " + std::to_string(Limits::MIN_NUM_COLORS) +
               " and " + std::to_string(Limits::MAX_NUM_COLORS);
    }
    if (!transparency && transparentThreshold > 0) {
        return "Transparent threshold must be 0 when transparency is disabled";
    }
    if (transparentThreshold > 255) {
        return "Transparent threshold must be between 0 and 255";
    }
    if (singleFrame && transparency) {
        return "Transparency should be disabled when generating a single frame GIF";
    }
    return std::nullopt;
}