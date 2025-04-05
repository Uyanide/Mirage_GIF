#ifndef GIFMIRAGE_GIF_OPTIONS_H
#define GIFMIRAGE_GIF_OPTIONS_H

#include <optional>
#include <string>

#include "def.h"

namespace GIFMirage {

enum class MergeMode : u32 {
    DIAG_2 = 0,
    ROW_1,
    COLUMN_1,
    BASE3_2,
    CHESSBOARD,
    PIVOT,  // not used

};

class Options {
    static constexpr auto mergeModeHint = "Merge mode (0: DIAG_2, 1: ROW_1, 2: COLUMN_1, 3: BASE3_2, 4: CHESSBOARD)";

    struct Defaults {
        static constexpr u32 width              = 640;
        static constexpr u32 height             = 640;
        static constexpr u32 frameCount         = 30;
        static constexpr u32 delay              = 80;
        static constexpr u32 mergeMode          = 0;
        static constexpr const char* outputFile = "output.gif";
        static constexpr u32 threadCount        = 0;  // 0 means auto-detect
    };

    struct Limits {
        static constexpr u32 width      = 4096;
        static constexpr u32 height     = 4096;
        static constexpr u32 frameCount = 1000;
        static constexpr u32 delay      = 65535;  // max of u16
    };

   public:
    std::string innerFile;
    std::string coverFile;
    std::string outputFile = Defaults::outputFile;
    u32 width              = Defaults::width;
    u32 height             = Defaults::height;
    u32 frameCount         = Defaults::frameCount;
    u32 delay              = Defaults::delay;
    u32 mergeMode          = 0;
    u32 threadCount        = Defaults::threadCount;

   public:
    static std::optional<Options>
    parseArgs(int argc, char** argv) noexcept;

    void
    checkValid() const;
};
}  // namespace GIFMirage

#endif  // GIFMIRAGE_GIF_OPTIONS_H