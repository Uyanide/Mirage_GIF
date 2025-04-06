#include <leptonica/allheaders.h>

#include <algorithm>
#include <numeric>
#include <ranges>
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
    explicit QuantizerException(const string&& msg) : message(msg) {}
    [[nodiscard]] const char*
    what() const noexcept override {
        return message.c_str();
    }
};

static vector<PixelBGRA>
shrinkPalette(const vector<PixelBGRA>& palette, vector<u8>& indices, u32 colorCount, u32 targetColorCount) noexcept {
    // find the top $targetColorCount used colors in the palette
    std::vector<std::pair<u32, u32>> count(colorCount);
    for (u32 i = 0; i < colorCount; i++) {
        count[i] = {i, 0};
    }
    for (const auto& index : indices) {
        count[index].second++;
    }
    std::ranges::sort(count, [](const auto& a, const auto& b) { return a.second > b.second; });
    // map the most used colors to the first $targetColorCount indices
    std::vector<u8> mapReplace(colorCount);
    for (auto it = count.begin(); it != count.begin() + targetColorCount; ++it) {
        mapReplace[it->first] = static_cast<u8>(it - count.begin());
    }
    for (auto it = count.begin() + targetColorCount; it != count.end(); ++it) {
        // find the closest color in the palette
        double minDist = -1;
        u8 minIdx      = 0;
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
    for (u32 i = 0; i < targetColorCount; i++) {
        newPalette[i] = palette[count[i].first];
    }
    return newPalette;
}

// ordered dithering with multiple (grayscale) colors
static void
orderedDithering(const vector<PixelBGRA>& pixelsAdjusted,
                 const vector<PixelBGRA>& sortedPalette,
                 const vector<PixelBGRA>& sortedPaletteGray,
                 vector<u8>& indices,
                 const u32 width,
                 const u32 height,
                 const u32 colorCount) noexcept {
    static constexpr double BayerMat[4][4] = {{0 / 16., 8 / 16., 2 / 16., 10 / 16.},
                                              {12 / 16., 4 / 16., 14 / 16., 6 / 16.},
                                              {3 / 16., 11 / 16., 1 / 16., 9 / 16.},
                                              {15 / 16., 7 / 16., 13 / 16., 5 / 16.}};

    for (u32 y = 0, idx = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++, idx++) {
            const u8 index = indices[idx];
            // skip transparent pixels
            if (!pixelsAdjusted[idx].a) {
                continue;
            }
            // exactly the same color
            if (pixelsAdjusted[idx] == sortedPalette[index]) continue;
            // compare in GrayScale
            const u32 grayPalette = sortedPaletteGray[index].r;
            // brighter
            if (const u32 grayOrig = pixelsAdjusted[idx].r; grayPalette > grayOrig) {
                // already the darkest
                if (index == 0) continue;
                const u32 grayPrev = sortedPaletteGray[index - 1].r;
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
                const u32 grayNext = sortedPaletteGray[index + 1].r;
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
                   u32 width,
                   u32 height,
                   u32 numColors,
                   DitherMode ditherMode,
                   bool grayScale,
                   bool transparency,
                   u8 transparentThreshold,
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

        // Adjust input pixels
        PIX* pixs = pixCreate(width, height, 32);
        if (!pixs) {
            throw QuantizerException("Failed to create PIX object");
        }
        vector<PixelBGRA> pixelsAdjusted(pixels.size());
        u32* data             = pixGetData(pixs);
        int32_t wpl           = pixGetWpl(pixs);
        bool isAllTransparent = true;
        for (u32 y = 0, idx = 0; y < height; y++) {
            if (!data) {
                throw QuantizerException("Failed to get data from PIX object");
            }
            u32* line = data + y * wpl;
            for (u32 x = 0; x < width; x++, idx++) {
                const auto& pixel = grayScale ? toGray(pixels[idx]) : pixels[idx];
                PixelBGRA argb;
                if (transparency && pixel.a <= transparentThreshold) {
                    argb = makeBGRA(0, 0, 0, 0);
                } else {
                    isAllTransparent = false;
                    argb             = preMultiply(pixel);
                }
                pixelsAdjusted[idx] = argb;
                line[x]             = (static_cast<u32>(argb.r) << 24) | (static_cast<u32>(argb.g) << 16) |
                          (static_cast<u32>(argb.b) << 8) | 255;
            }
        }

        // If all pixels are transparent
        if (isAllTransparent) {
            GeneralLogger::warning("All pixels are considered as transparent, skipping quantization.");
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
        for (u32 i = 0; i < colorCount; i++) {
            l_int32 r, g, b;
            pixcmapGetColor(cmap, i, &r, &g, &b);
            palette[i]     = makeBGRA(static_cast<u8>(b), static_cast<u8>(g), static_cast<u8>(r), 0xFFu);
            paletteGray[i] = toGray(palette[i]);
            if (grayScale) {
                palette[i] = paletteGray[i];
            }
        }

        // Sort palette and create new indices if needed
        vector<u8> newIdx(colorCount);
        vector<PixelBGRA> sortedPalette(colorCount);
        vector<PixelBGRA> sortedPaletteGray(colorCount);
        if (ditherMode == DitherOrdered) {
            vector<u8> sortedIdx(colorCount);
            std::iota(sortedIdx.begin(), sortedIdx.end(), 0);
            std::ranges::sort(sortedIdx, [&paletteGray](u8 a, u8 b) { return paletteGray[a].r < paletteGray[b].r; });
            for (u32 i = 0; i < colorCount; i++) {
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
        vector<u8> indices(width * height);
        bool warned = false;
        data        = pixGetData(pixd);
        wpl         = pixGetWpl(pixd);
        for (u32 y = 0, idx = 0; y < height; y++) {
            if (!data) {
                throw QuantizerException("Failed to get data from quantized PIX object");
            }
            u32* line = data + y * wpl;
            for (u32 x = 0; x < width; x++, idx++) {
                u8 byte = GET_DATA_BYTE(line, x);
                if (byte >= colorCount) {
                    byte = colorCount - 1;
                    if (!warned) {
                        GeneralLogger::warning("Index out of bounds, using last color in palette");
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
            GeneralLogger::warning("Ordered dithering works poorly for color images, skipping dithering.");
        } else if (ditherMode == DitherOrdered && grayScale) {
            orderedDithering(pixelsAdjusted, sortedPalette, sortedPaletteGray, indices, width, height, colorCount);
        }

        // Known issue with Leptonica: if numColors == 2, the palette
        // may contain 3 colors, but only 2 are required.
        if (colorCount > numColors) {
            GeneralLogger::warning("Unexpected number of colors in palette: " + std::to_string(colorCount) +
                                   ", expected: " + std::to_string(numColors));
            GeneralLogger::warning("Shrinking palette to " + std::to_string(numColors) + " colors...");
            sortedPalette = shrinkPalette(sortedPalette, indices, colorCount, numColors);
        }
        // simply fill with black
        else if (colorCount < numColors) {
            GeneralLogger::warning("Unexpected number of colors in palette: " + std::to_string(colorCount) +
                                   ", expected: " + std::to_string(numColors));
            GeneralLogger::warning("Filling palette with black...");
            sortedPalette.resize(numColors, makeBGRA(0, 0, 0, 255));
        }

        // Set the transparency index, if needed
        if (transparency) {
            for (u32 i = 0; i < indices.size(); i++) {
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
GIFImage::findUnusedColor(const span<PixelBGRA>& pixels, const u32 step) noexcept {
    std::unordered_set<PixelBGRA, PixelBRAGHash> usedColors{pixels.begin(), pixels.end()};
    for (u32 r = 0; r < 256; r += step) {
        for (u32 g = 0; g < 256; g += step) {
            for (u32 b = 0; b < 256; b += step) {
                PixelBGRA color = makeBGRA(b, g, r, 255);
                if (usedColors.find(color) == usedColors.end()) {
                    return color;
                }
            }
        }
    }
    return std::nullopt;  // No unused color found
}