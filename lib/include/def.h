#ifndef GIF_DEF_H
#define GIF_DEF_H

#ifdef _MSC_VER

#define EXPORT __declspec(dllexport)
#define PACKED

#else  // _MSC_VER

#define EXPORT __attribute__((visibility("default")))
#define PACKED __attribute__((packed))
#endif  // _MSC_VER

#ifndef __cplusplus

#define EXTERN_C

#include <stdint.h>

#else  // __cplusplus

#define EXTERN_C extern "C"

#ifndef GIF_TYPES_DEFINED
#define GIF_TYPES_DEFINED

#include <cstdint>

inline constexpr uint8_t
TOU8(const uint32_t x) {
    return static_cast<uint8_t>(x & 0xFF);
}

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif  // _MSC_VER

struct PixelBGRA {
    uint8_t b, g, r, a;

    inline bool
    operator==(const PixelBGRA& other) const {
        return b == other.b && g == other.g && r == other.r && a == other.a;
    }

    [[nodiscard]] inline uint32_t
    toU32() const {
        return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) |
               static_cast<uint32_t>(b);
    }
} PACKED;

static_assert(sizeof(PixelBGRA) == 4, "PixelBGRA size is not 4 bytes!");

#ifdef _MSC_VER
#pragma pack(pop)
#endif  // _MSC_VER

struct PixelBRAGHash {
    std::size_t
    operator()(const PixelBGRA& pixel) const {
        return std::hash<uint32_t>()(pixel.toU32());
    }
};

inline constexpr PixelBGRA
makeBGRA(const uint8_t b, const uint8_t g, const uint8_t r, const uint8_t a = 0xff) {
    return {b, g, r, a};
}

inline PixelBGRA
toGray(const PixelBGRA& p) {
    uint8_t l = TOU8((p.r * 76u + p.g * 151u + p.b * 29u) >> 8);
    return makeBGRA(l, l, l, p.a);
}

inline PixelBGRA
preMultiply(const PixelBGRA& p) {
    uint32_t a = p.a;
    if (a == 0) return makeBGRA(0, 0, 0, 0xffu);
    if (a == 255) return p;
    uint8_t r = TOU8((p.a * a) >> 8);
    uint8_t g = TOU8((p.g * a) >> 8);
    uint8_t b = TOU8((p.b * a) >> 8);
    return makeBGRA(b, g, r, 0xffu);
}

/**
 * @return square of distance, approximately in [0, 600,000)
 * @details no sqrt are applied, since sometimes we only need to compare the distances
 */
inline double
colorDistance(const PixelBGRA& e1, const PixelBGRA& e2) {
    int32_t rmean = ((int32_t)e1.r + (int32_t)e2.g) / 2;
    int32_t r     = (int32_t)e1.r - (int32_t)e2.r;
    int32_t g     = (int32_t)e1.g - (int32_t)e2.g;
    int32_t b     = (int32_t)e1.b - (int32_t)e2.b;
    return (((512 + rmean) * r * r) >> 8) + 4 * g * g + (((767 - rmean) * b * b) >> 8);
}

#include <cmath>

/**
 * @brief distance, approximately in [0, 800)
 * @details but other times we do need the linear distances
 */
inline double
colorDistanceSqrt(const PixelBGRA& e1, const PixelBGRA& e2) {
    return std::sqrt(colorDistance(e1, e2));
}

/**
 * @return hue in [0, 360)
 */
inline int32_t
getHue(const PixelBGRA& p) {
    int32_t r   = p.r;
    int32_t g   = p.g;
    int32_t b   = p.b;
    int32_t max = r < g ? (g < b ? b : g) : (r < b ? b : r);
    int32_t min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    if (max == min) return 0;  // achromatic
    if (max == r) return ((g - b) * 60) / (max - min);
    if (max == g) return ((b - r) * 60 + 120) / (max - min);
    return ((r - g) * 60 + 240) / (max - min);
}

#endif  // GIF_TYPES_DEFINED

#endif  // __cplusplus

#endif  // GIF_DEF_H