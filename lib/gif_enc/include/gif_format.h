#ifndef GIF_FORMAT_H
#define GIF_FORMAT_H

#include <string>
#include <vector>

#include "def.h"

namespace GIFEnc {
constexpr u8 GIF_COLOR_RES      = 8;  // color resolution
constexpr u8 GIF_DISPOSE_METHOD = 3;  // dispose method
constexpr u8 GIF_END            = 0x3B;

std::vector<u8>
gifHeader(u32 width,
          u32 height,
          u32 backgroundIndex,
          u32 minCodeLength,
          u32 loops,
          const bool hasGlobalColorTable,
          const std::vector<PixelBGRA> &globalColorTable = {}) noexcept;

std::vector<u8>
gifFrameHeader(u32 width,
               u32 height,
               u32 delay,
               bool hasTransparency,
               u32 transparentIndex,
               u32 minCodeLength,
               const std::vector<PixelBGRA> &palette = {}) noexcept;

std::vector<u8>
gifApplicationExtension(const std::string &identifier,
                        const std::string &authentication,
                        const std::vector<u8> &data) noexcept;
};  // namespace GIFEnc

#endif  // GIF_FORMAT_H