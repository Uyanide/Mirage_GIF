#ifndef IMAGE_SEQUENCE_DITHER_H
#define IMAGE_SEQUENCE_DITHER_H

#include <array>
#include <cstring>

#include "def.h"

namespace ImageSequence {
namespace Dither {

template <u8 size>
class BayerOrderedDithering {
   public:
    static void
    orderedDithering(u8* out, const u8* in, u32 width, u32 height) {
        // do nothing
        return;
    }
};

template <>
class BayerOrderedDithering<4> {
   private:
    // {{{0, 8, 2, 10}, {12, 4, 14, 6}, {3, 11, 1, 9}, {15, 7, 13, 5}}}
    static constexpr std::array<std::array<double, 4>, 4> BAYER_MATRIX{{{0, 127.5, 31.875, 159.375},
                                                                        {191.25, 63.75, 223.125, 95.625},
                                                                        {47.8125, 175.3125, 15.9375, 143.4375},
                                                                        {239.0625, 111.5625, 207.1875, 79.6875}}};

   public:
    static void
    orderedDithering(u8* out, const PixelBGRA* data, u32 width, u32 height) noexcept {
        const auto argb = data;
        for (u32 y = 0, idx = 0; y < height; y++) {
            for (u32 x = 0; x < width; x++, idx++) {
                const auto v = *(argb + idx);
                const auto l = toGray(v).r;
                out[idx]     = double(l) > (BAYER_MATRIX[x & 3][y & 3]) ? 255 : 0;
            }
        }
    }
};

}  // namespace Dither
}  // namespace ImageSequence

#endif  // IMAGE_SEQUENCE_DITHER_H