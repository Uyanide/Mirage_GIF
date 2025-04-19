#ifndef GIFLSB_OPTIONS_H
#define GIFLSB_OPTIONS_H

#include <cstdint>
#include <optional>
#include <string>

#include "file_reader.h"
#include "file_writer.h"
#include "imsq.h"
#include "imsq_stream.h"

namespace GIFLsb {

class DecodeOptions {
  public:
    GIFImage::ImageSequenceStream::Ref image;
    std::string imagePath;
    NaiveIO::FileWriter::Ref outputFile;
    std::string outputName;
    std::string outputDirectory;  // ends with '/'
    std::string tempFileName;

  public:
    static std::optional<DecodeOptions>
    parseArgs(int argc, char** argv) noexcept;

    void
    ensureValid();
};

class EncodeOptions {
    struct Defaults {
        static constexpr uint32_t NUM_COLORS            = 15;
        static constexpr uint32_t TRANSPARENT_THRESHOLD = 0;
        static constexpr const char* OUTPUT_FILE        = "encrypted.gif";
        static constexpr uint32_t THREAD_COUNT          = 0;  // 0 means auto-detect
        static constexpr double MARK_RATIO              = 0.04;
    };

    struct Limits {
        static constexpr uint32_t MIN_NUM_COLORS = 2;
        static constexpr uint32_t MAX_NUM_COLORS = 31;
        static constexpr double MAX_MARK_RATIO   = 0.5;
    };

  public:
    GIFImage::ImageSequence::Ref image;
    NaiveIO::FileReader::Ref file;
    NaiveIO::FileWriter::Ref outputFile;
    std::string imagePath;
    std::string filePath;
    std::string markText;
    bool disableDither            = false;
    bool transparency             = false;
    bool grayscale                = false;
    bool enableLocalPalette       = false;
    bool singleFrame              = false;
    std::string outputPath        = Defaults::OUTPUT_FILE;
    uint32_t numColors            = Defaults::NUM_COLORS;
    uint32_t transparentThreshold = Defaults::TRANSPARENT_THRESHOLD;
    uint32_t threadCount          = Defaults::THREAD_COUNT;
    double markRatio              = Defaults::MARK_RATIO;

  public:
    static std::optional<EncodeOptions>
    parseArgs(int argc, char** argv) noexcept;

    void
    ensureValid();
};
}  // namespace GIFLsb

#endif  // GIFLSB_OPTIONS_H