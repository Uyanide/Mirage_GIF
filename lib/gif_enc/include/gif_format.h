#ifndef GIF_FORMAT_H
#define GIF_FORMAT_H

#include <string>
#include <vector>

#include "def.h"

namespace GIFEnc {
constexpr uint8_t GIF_COLOR_RES      = 8;  // color resolution
constexpr uint8_t GIF_DISPOSE_METHOD = 3;  // dispose method
constexpr uint8_t GIF_END            = 0x3B;

std::vector<uint8_t>
gifHeader(uint32_t width,
          uint32_t height,
          uint32_t backgroundIndex,
          uint32_t minCodeLength,
          uint32_t loops,
          bool hasGlobalColorTable,
          const std::vector<PixelBGRA> &globalColorTable = {}) noexcept;

std::vector<uint8_t>
gifFrameHeader(uint32_t width,
               uint32_t height,
               uint32_t delay,
               bool hasTransparency,
               uint32_t transparentIndex,
               uint32_t disposalMethod,
               uint32_t minCodeLength,
               const std::vector<PixelBGRA> &palette = {}) noexcept;

std::vector<uint8_t>
gifApplicationExtension(const std::string &identifier,
                        const std::string &authentication,
                        const std::vector<uint8_t> &data) noexcept;
};  // namespace GIFEnc

#endif  // GIF_FORMAT_H