#ifndef GIFMIRAGE_GIF_OPTIONS_H
#define GIFMIRAGE_GIF_OPTIONS_H

#include <optional>
#include <string>

#include "def.h"

namespace GIFMirage {

struct MergeMode {
    u32 slope  = 0;     // 0-4
    u32 width  = 0;     // 0-4
    bool isRow = true;  // true: row, false: column

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
        static constexpr u32 width              = 640;
        static constexpr u32 height             = 640;
        static constexpr u32 frameCount         = 30;
        static constexpr u32 delay              = 80;
        static constexpr std::string mergeMode  = "S2W1C";
        static constexpr const char* outputFile = "output.gif";
        static constexpr u32 threadCount        = 0;  // 0 means auto-detect
        static constexpr u32 disposalMethod     = 3;
    };

    struct Limits {
        static constexpr u32 width          = 4096;
        static constexpr u32 height         = 4096;
        static constexpr u32 frameCount     = 1000;
        static constexpr u32 delay          = 65535;  // max of u16
        static constexpr u32 modeSlope      = 4;
        static constexpr u32 modeWidth      = 4;
        static constexpr u32 disposalMethod = 3;
    };

    std::string innerFile;
    std::string coverFile;
    std::string outputFile = Defaults::outputFile;
    u32 width              = Defaults::width;
    u32 height             = Defaults::height;
    u32 frameCount         = Defaults::frameCount;
    u32 delay              = Defaults::delay;
    MergeMode mergeMode;
    u32 threadCount    = Defaults::threadCount;
    u32 disposalMethod = Defaults::disposalMethod;

   public:
    static std::optional<Options>
    parseArgs(int argc, char** argv) noexcept;

    void
    checkValid() const;
};
}  // namespace GIFMirage

#endif  // GIFMIRAGE_GIF_OPTIONS_H