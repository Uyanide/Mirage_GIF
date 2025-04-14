#ifndef IMAGE_SEQUENCE_DITHER_H
#define IMAGE_SEQUENCE_DITHER_H

#include <array>

#include "def.h"


namespace ImageSequence::Dither {

template <uint8_t size>
class BayerOrderedDithering {
  public:
    static void
    orderedDithering(uint8_t* out, const uint8_t* in, uint32_t width, uint32_t height) {
        // do nothing
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
    orderedDithering(uint8_t* out, const PixelBGRA* data, uint32_t width, uint32_t height) noexcept {
        const auto argb = data;
        for (uint32_t y = 0, idx = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++, idx++) {
                const auto v = *(argb + idx);
                const auto l = toGray(v).r;
                out[idx]     = double(l) > (BAYER_MATRIX[x & 3][y & 3]) ? 255 : 0;
            }
        }
    }
};

} // namespace ImageSequence::Dither


#endif  // IMAGE_SEQUENCE_DITHER_H