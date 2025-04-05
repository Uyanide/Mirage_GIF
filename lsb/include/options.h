#ifndef GIFLSB_OPTIONS_H
#define GIFLSB_OPTIONS_H

#include <optional>
#include <string>

#include "def.h"

namespace GIFLsb {

class DecodeOptions {
   public:
    std::string decyptImage;
    std::string outputFile;

   public:
    static std::optional<DecodeOptions>
    parseArgs(int argc, char** argv) noexcept;
};

class EncodeOptions {
    struct Defaults {
        static constexpr const char* MARK_TEXT     = "MTK";
        static constexpr u32 NUM_COLORS            = 15;
        static constexpr u32 TRANSPARENT_THRESHOLD = 0;
        static constexpr const char* OUTPUT_FILE   = "encrypted.gif";
        static constexpr u32 THREAD_COUNT          = 0;  // 0 means auto-detect
    };

    struct Limits {
        static constexpr u32 MIN_NUM_COLORS = 2;
        static constexpr u32 MAX_NUM_COLORS = 31;
    };

   public:
    std::string imageFile;
    std::string encyptFile;
    std::string markText     = Defaults::MARK_TEXT;
    bool disableDither       = false;
    bool transparency        = false;
    bool grayscale           = false;
    bool enableLocalPalette  = false;
    std::string outputFile   = Defaults::OUTPUT_FILE;
    u32 numColors            = Defaults::NUM_COLORS;
    u32 transparentThreshold = Defaults::TRANSPARENT_THRESHOLD;
    u32 threadCount          = Defaults::THREAD_COUNT;

   public:
    static std::optional<EncodeOptions>
    parseArgs(int argc, char** argv) noexcept;

    void
    checkValid() const;
};
}  // namespace GIFLsb

#endif  // GIFLSB_OPTIONS_H