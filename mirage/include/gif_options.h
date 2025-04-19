#ifndef GIFMIRAGE_GIF_OPTIONS_H
#define GIFMIRAGE_GIF_OPTIONS_H

#include <cstdint>
#include <optional>
#include <string>

#include "file_writer.h"
#include "imsq.h"

namespace GIFMirage {

struct MergeMode {
    uint32_t slope = 0;     // 0-4
    uint32_t width = 0;     // 0-4
    bool isRow     = true;  // true: row, false: column

    static std::optional<MergeMode>
    parse(const std::string& str) noexcept;

    std::string
    toString() const noexcept {
        return "S" + std::to_string(slope) + "W" + std::to_string(width) + (isRow ? "R" : "C");
    }
};

class Options {
    static constexpr auto mergeModeHint =
        "Merge mode (^S(\\d+)W(\\d+)([CR])$):\n"
        "  S: Slope, [0, 4]\n"
        "  W: Width, [1, 4]\n"
        "  C/R: Direction, Column/Row\n"
        "  (e.g. S2W1R = Slope 1, Width 2, Row)";

  public:
    struct Defaults {
        static constexpr uint32_t width          = 640;
        static constexpr uint32_t height         = 640;
        static constexpr uint32_t frameCount     = 30;
        static constexpr uint32_t delay          = 80;
        static constexpr std::string mergeMode   = "S2W1C";
        static constexpr const char* outputPath  = "output.gif";
        static constexpr uint32_t threadCount    = 0;  // 0 means auto-detect
        static constexpr uint32_t disposalMethod = 3;
    };

    struct Limits {
        static constexpr uint32_t width          = 4096;
        static constexpr uint32_t height         = 4096;
        static constexpr uint32_t frameCount     = 1000;
        static constexpr uint32_t delay          = 65535;  // max of u16
        static constexpr uint32_t modeSlope      = 4;
        static constexpr uint32_t modeWidth      = 4;
        static constexpr uint32_t disposalMethod = 3;
    };

    GIFImage::ImageSequence::Ref innerImage;
    GIFImage::ImageSequence::Ref coverImage;
    std::string innerPath;
    std::string coverPath;
    NaiveIO::FileWriter::Ref outputFile;
    std::string outputPath = Defaults::outputPath;
    uint32_t width         = Defaults::width;
    uint32_t height        = Defaults::height;
    uint32_t frameCount    = Defaults::frameCount;
    uint32_t delay         = Defaults::delay;
    MergeMode mergeMode;
    uint32_t threadCount    = Defaults::threadCount;
    uint32_t disposalMethod = Defaults::disposalMethod;

  public:
    static std::optional<Options>
    parseArgs(int argc, char** argv) noexcept;

    void
    ensureValid() const;
};
}  // namespace GIFMirage

#endif  // GIFMIRAGE_GIF_OPTIONS_H