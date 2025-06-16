#if 0
#error "Deprecated, use native implementation instead."
#else
#include <allheaders.h>

#include <algorithm>
#include <climits>
#include <numeric>
#include <string>
#include <unordered_set>

#include "log.h"
#include "quantizer.h"

using namespace GIFImage;
using std::span, std::vector;
using std::string;

class QuantizerException : public std::exception {
    const string message;

  public:
    explicit QuantizerException(const string&& msg)
        : message(msg) {}

    [[nodiscard]] const char*
    what() const noexcept override {
        return message.c_str();
    }
};

static vector<PixelBGRA>
shrinkPalette(const vector<PixelBGRA>& palette, vector<uint8_t>& indices, uint32_t colorCount, uint32_t targetColorCount) noexcept {
    // find the top $targetColorCount used colors in the palette
    std::vector<std::pair<uint32_t, uint32_t> > count(colorCount);
    for (uint32_t i = 0; i < colorCount; i++) {
        count[i] = {i, 0};
    }
    for (const auto& index : indices) {
        count[index].second++;
    }
    std::ranges::sort(count, [](const auto& a, const auto& b) { return a.second > b.second; });
    // map the most used colors to the first $targetColorCount indices
    std::vector<uint8_t> mapReplace(colorCount);
    for (auto it = count.begin(); it != count.begin() + targetColorCount; ++it) {
        mapReplace[it->first] = static_cast<uint8_t>(it - count.begin());
    }
    for (auto it = count.begin() + targetColorCount; it != count.end(); ++it) {
        // find the closest color in the palette
        double minDist = -1;
        uint8_t minIdx = 0;
        // mostly not the bottleneck, so rough calculations of distance is acceptable
        for (auto prev = count.begin(); prev < it; ++prev) {
            if (const double dist = colorDistance(palette[prev->first], palette[it->first]);
                minDist == -1 || dist < minDist) {
                minDist = dist;
                minIdx  = prev->first;
            }
        }
        // replace the color with the closest one
        mapReplace[it->first] = mapReplace[minIdx];
    }
    // replace the indices with the new ones
    std::ranges::for_each(indices, [&mapReplace](auto& index) { index = mapReplace[index]; });
    // generate the new palette
    vector<PixelBGRA> newPalette(targetColorCount);
    for (uint32_t i = 0; i < targetColorCount; i++) {
        newPalette[i] = palette[count[i].first];
    }
    return newPalette;
}

// ordered dithering with multiple (grayscale) colors
static void
orderedDithering(const vector<PixelBGRA>& pixelsAdjusted,
                 const vector<PixelBGRA>& sortedPalette,
                 const vector<PixelBGRA>& sortedPaletteGray,
                 vector<uint8_t>& indices,
                 const uint32_t width,
                 const uint32_t height,
                 const uint32_t colorCount) noexcept {
    static constexpr double BayerMat[4][4] = {{0 / 16., 8 / 16., 2 / 16., 10 / 16.},
                                              {12 / 16., 4 / 16., 14 / 16., 6 / 16.},
                                              {3 / 16., 11 / 16., 1 / 16., 9 / 16.},
                                              {15 / 16., 7 / 16., 13 / 16., 5 / 16.}};

    for (uint32_t y = 0, idx = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++, idx++) {
            const uint8_t index = indices[idx];
            // skip transparent pixels
            if (!pixelsAdjusted[idx].a) {
                continue;
            }
            // exactly the same color
            if (pixelsAdjusted[idx] == sortedPalette[index]) continue;
            // compare in GrayScale
            const uint32_t grayPalette = sortedPaletteGray[index].r;
            // brighter
            if (const uint32_t grayOrig = pixelsAdjusted[idx].r; grayPalette > grayOrig) {
                // already the darkest
                if (index == 0) continue;
                const uint32_t grayPrev = sortedPaletteGray[index - 1].r;
                if (grayPalette == grayPrev) {
                    indices[idx] = index - 1;  // no dithering needed, just use the previous color
                    continue;
                }
                if (const double ratio = static_cast<double>(grayOrig - grayPrev) / (grayPalette - grayPrev);
                    ratio < BayerMat[y % 4][x % 4]) {
                    indices[idx] = index - 1;
                } else {
                    indices[idx] = index;
                }
            }
            // darker
            else {
                // already the brightest
                if (index == colorCount - 1) continue;
                const uint32_t grayNext = sortedPaletteGray[index + 1].r;
                if (grayPalette == grayNext) {
                    indices[idx] = index + 1;  // no dithering needed, just use the next color
                    continue;
                }
                if (const double ratio = static_cast<double>(grayOrig - grayPalette) / (grayNext - grayPalette);
                    ratio >= BayerMat[y % 4][x % 4]) {
                    indices[idx] = index + 1;
                } else {
                    indices[idx] = index;
                }
            }
        }
    }
}

GIFImage::QuantizerResult
GIFImage::quantize(const span<PixelBGRA>& pixels,
                   uint32_t width,
                   uint32_t height,
                   uint32_t numColors,
                   DitherMode ditherMode,
                   bool grayScale,
                   bool transparency,
                   uint8_t transparentThreshold,
                   bool downsample) noexcept {
    try {
        // parameter validation
        if (numColors < 2 || numColors > 256) {
            throw QuantizerException("numColors out of range: " + std::to_string(numColors) +
                                     ", must be between 2 and 256");
        }
        if (numColors == 256 && transparency) {
            throw QuantizerException("Transparency is not supported with 256 colors");
        }
        if (width == 0 || height == 0) {
            throw QuantizerException("Image dimensions must be greater than 0");
        }
        if (pixels.size() != width * height) {
            throw QuantizerException("Pixel data size does not match image dimensions: " +
                                     std::to_string(pixels.size()) + " != " + std::to_string(width * height));
        }
        if (pixels.size() >= UINT_MAX) {
            throw QuantizerException("Pixel data size exceeds maximum limit: " + std::to_string(pixels.size()));
        }

        // Adjust input pixels
        PIX* pixs = pixCreate(width, height, 32);
        if (!pixs) {
            throw QuantizerException("Failed to create PIX object");
        }
        vector<PixelBGRA> pixelsAdjusted(pixels.size());
        uint32_t* data        = pixGetData(pixs);
        int32_t wpl           = pixGetWpl(pixs);
        bool isAllTransparent = true;
        for (uint32_t y = 0, idx = 0; y < height; y++) {
            if (!data) {
                throw QuantizerException("Failed to get data from PIX object");
            }
            uint32_t* line = data + y * wpl;
            for (uint32_t x = 0; x < width; x++, idx++) {
                const auto& pixel = grayScale ? toGray(pixels[idx]) : pixels[idx];
                PixelBGRA argb;
                if (transparency && pixel.a <= transparentThreshold) {
                    argb = makeBGRA(0, 0, 0, 0);
                } else {
                    isAllTransparent = false;
                    argb             = preMultiply(pixel);
                }
                pixelsAdjusted[idx] = argb;
                line[x]             = (static_cast<uint32_t>(argb.r) << 24) | (static_cast<uint32_t>(argb.g) << 16) |
                          (static_cast<uint32_t>(argb.b) << 8) | 255;
            }
        }

        // If all pixels are transparent
        if (isAllTransparent) {
            GeneralLogger::warn("All pixels are considered as transparent, skipping quantization.");
            return QuantizerResult{
                .isValid      = false,
                .errorMessage = "All pixels are transparent",
            };
        }

        // Apply quantization, and Floyd-Steinberg dithering if needed
        PIX* pixd =
            pixMedianCutQuantGeneral(pixs, ditherMode == DitherFloydSteinberg, 8, numColors, 0, downsample ? 0 : 1, 0);
        if (!pixd) {
            pixDestroy(&pixs);
            throw QuantizerException("Quantization failed");
        }

        // Obtain colormap and palette
        PIXCMAP* cmap = pixGetColormap(pixd);
        if (!cmap) {
            pixDestroy(&pixs);
            pixDestroy(&pixd);
            throw QuantizerException("No colormap in quantized image");
        }
        const auto colorCount = pixcmapGetCount(cmap);
        vector<PixelBGRA> palette(colorCount);
        vector<PixelBGRA> paletteGray(colorCount);  // for sorting
        for (uint32_t i = 0; i < colorCount; i++) {
            l_int32 r, g, b;
            pixcmapGetColor(cmap, i, &r, &g, &b);
            palette[i]     = makeBGRA(static_cast<uint8_t>(b), static_cast<uint8_t>(g), static_cast<uint8_t>(r), 0xFFu);
            paletteGray[i] = toGray(palette[i]);
            if (grayScale) {
                palette[i] = paletteGray[i];
            }
        }

        // Sort palette and create new indices if needed
        vector<uint8_t> newIdx(colorCount);
        vector<PixelBGRA> sortedPalette(colorCount);
        vector<PixelBGRA> sortedPaletteGray(colorCount);
        if (ditherMode == DitherOrdered) {
            vector<uint8_t> sortedIdx(colorCount);
            std::iota(sortedIdx.begin(), sortedIdx.end(), 0);
            std::ranges::sort(sortedIdx, [&paletteGray](uint8_t a, uint8_t b) { return paletteGray[a].r < paletteGray[b].r; });
            for (uint32_t i = 0; i < colorCount; i++) {
                newIdx[sortedIdx[i]] = i;
                sortedPalette[i]     = palette[sortedIdx[i]];
                sortedPaletteGray[i] = paletteGray[sortedIdx[i]];
            }
        } else {
            std::iota(newIdx.begin(), newIdx.end(), 0);
            sortedPalette     = palette;
            sortedPaletteGray = paletteGray;
        }

        // Get indices, the indices will be converted to fit the sorted palette
        vector<uint8_t> indices(width * height);
        bool warned = false;
        data        = pixGetData(pixd);
        wpl         = pixGetWpl(pixd);
        for (uint32_t y = 0, idx = 0; y < height; y++) {
            if (!data) {
                throw QuantizerException("Failed to get data from quantized PIX object");
            }
            uint32_t* line = data + y * wpl;
            for (uint32_t x = 0; x < width; x++, idx++) {
                uint8_t byte = GET_DATA_BYTE(line, x);
                if (byte >= colorCount) {
                    byte = colorCount - 1;
                    if (!warned) {
                        GeneralLogger::warn("Index out of bounds, using last color in palette");
                        warned = true;
                    }
                }
                indices[idx] = newIdx[byte];
            }
        }

        // Apply ordered dithering if needed
        if (ditherMode == DitherOrdered && !grayScale) {
            // It is indeed possible to convert colors to grayscale values, dither by grayscale and map back
            // into RGB space. To avoid distortion a threshold to the Color perception distance could be applied.
            // However, I didn't manage to find the right parameter for this approach, so ...
            GeneralLogger::warn("Ordered dithering works poorly for color images, skipping dithering.");
        } else if (ditherMode == DitherOrdered && grayScale) {
            orderedDithering(pixelsAdjusted, sortedPalette, sortedPaletteGray, indices, width, height, colorCount);
        }

        // Known issue with Leptonica: if numColors == 2, the palette
        // may contain 3 colors, but only 2 are required.
        if (colorCount > numColors) {
            GeneralLogger::warn("Unexpected number of colors in palette: " + std::to_string(colorCount) +
                                ", expected: " + std::to_string(numColors));
            GeneralLogger::warn("Shrinking palette to " + std::to_string(numColors) + " colors...");
            sortedPalette = shrinkPalette(sortedPalette, indices, colorCount, numColors);
        }
        // simply fill with black
        else if (colorCount < numColors) {
            GeneralLogger::warn("Unexpected number of colors in palette: " + std::to_string(colorCount) +
                                ", expected: " + std::to_string(numColors));
            GeneralLogger::warn("Filling palette with black...");
            sortedPalette.resize(numColors, makeBGRA(0, 0, 0, 255));
        }

        // Set the transparency index, if needed
        if (transparency) {
            for (uint32_t i = 0; i < indices.size(); i++) {
                if (!pixelsAdjusted[i].a) {
                    indices[i] = numColors;
                }
            }
        }

        pixDestroy(&pixs);
        pixDestroy(&pixd);

        return QuantizerResult{
            .isValid           = true,
            .palette           = std::move(sortedPalette),
            .indices           = std::move(indices),
            .hasTransparency   = transparency,
            .width             = width,
            .height            = height,
            .transparencyIndex = transparency ? numColors : 0,
            .errorMessage      = "",
        };

    } catch (const QuantizerException& e) {
        QuantizerResult result;
        result.isValid      = false;
        result.errorMessage = e.what();
        return result;
    } catch (const std::exception& e) {
        QuantizerResult result;
        result.isValid      = false;
        result.errorMessage = e.what();
        return result;
    } catch (...) {
        QuantizerResult result;
        result.isValid      = false;
        result.errorMessage = "Unknown error";
        return result;
    }
}

std::optional<PixelBGRA>
GIFImage::findUnusedColor(const span<PixelBGRA>& pixels, const uint32_t step) noexcept {
    std::unordered_set<PixelBGRA, PixelBGRAHash> usedColors{pixels.begin(), pixels.end()};
    for (uint32_t r = 0; r < 256; r += step) {
        for (uint32_t g = 0; g < 256; g += step) {
            for (uint32_t b = 0; b < 256; b += step) {
                PixelBGRA color = makeBGRA(b, g, r, 255);
                if (usedColors.find(color) == usedColors.end()) {
                    return color;
                }
            }
        }
    }
    return std::nullopt;  // No unused color found
}

#endif  // 1