#ifndef GIFIMAGE_QUANTIZER_H
#define GIFIMAGE_QUANTIZER_H

#include <optional>
#include <span>
#include <vector>

#include "def.h"
namespace GIFImage {

enum DitherMode {
    DitherNone,
    DitherFloydSteinberg,
    DitherOrdered,
};

struct QuantizerResult {
    bool isValid;
    std::vector<PixelBGRA> palette;
    std::vector<u8> indices;
    bool hasTransparency;
    u32 width;
    u32 height;
    u32 transparencyIndex;
    std::string errorMessage;
};

/**
 * @brief Generic quantizer interface.
 *
 * @param[in] pixels        The input pixel data to be quantized.
 *                          PixelARGB should be considered as BGRA8888 in
 *                          uint8_t* format with 4 channels due to endianness.
 * @param[in] width         The width of the image.
 * @param[in] height        The height of the image.
 * @param[in] numColors     The maximum number of colors to be used in the palette.
 *                          The size of the returned palette will always be same as
 *                          numColors.
 * @param[in] ditherMode    The dithering mode to be used. Default is DitherFloydSteinberg.
 * @param[in] grayScale     Whether to convert the image to grayscale before quantization.
 *                          Default is false.
 * @param[in] transparency  Whether to consider transparency in the quantization process.
 *                          If true, only the pixels with alpha value <= @p transparentThreshold
 *                          will be considered as transparent, which will be set to @p numColors
 *                          in the returned indices.
 *                          If false, pre-multiplication will be applied to all pixels.
 *                          Default is false.
 * @param[in] transparentThreshold
 * @param[in] downsample    Whether to downsample the image before quantization.
 *                          May not have any impact depending on actual implementation.
 *                          Default is true.
 *
 * @return @see QuantizerResult
 *         If error, errorMessage will be non-empty.
 */
QuantizerResult
quantize(const std::span<PixelBGRA>& pixels,
         u32 width,
         u32 height,
         u32 numColors,
         DitherMode ditherMode   = DitherFloydSteinberg,
         bool grayScale          = false,
         bool transparency       = false,
         u8 transparentThreshold = 0,
         bool downsample         = true) noexcept;

std::optional<PixelBGRA>
findUnusedColor(const std::span<PixelBGRA>& pixels, u32 step = 16) noexcept;

}  // namespace GIFImage

#endif  // GIFIMAGE_QUANTIZER_H