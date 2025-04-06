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

#ifndef INT_TYPES_DEFINED
#define INT_TYPES_DEFINED

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;
typedef int32_t i32;
typedef uint32_t u32;

#endif  // INT_TYPES_DEFINED

#else  // __cplusplus

#define EXTERN_C extern "C"

#ifndef GIF_TYPES_DEFINED
#define GIF_TYPES_DEFINED

#include <cstdint>

using u8  = uint8_t;
using u16 = uint16_t;
using u64 = uint64_t;
using i32 = int32_t;
using u32 = uint32_t;

static inline constexpr u8
TOU8(const u32 x) {
    return static_cast<u8>(x & 0xFF);
}

#ifdef _MSC_VER
#pragma pack(push, 1)
#endif  // _MSC_VER

struct PixelBGRA {
    u8 b, g, r, a;

    inline bool
    operator==(const PixelBGRA& other) const {
        return b == other.b && g == other.g && r == other.r && a == other.a;
    }

    [[nodiscard]] inline u32
    toU32() const {
        return (static_cast<u32>(a) << 24) | (static_cast<u32>(r) << 16) | (static_cast<u32>(g) << 8) |
               static_cast<u32>(b);
    }
} PACKED;

static_assert(sizeof(PixelBGRA) == 4, "PixelBGRA size is not 4 bytes!");

#ifdef _MSC_VER
#pragma pack(pop)
#endif  // _MSC_VER

struct PixelBRAGHash {
    std::size_t
    operator()(const PixelBGRA& pixel) const {
        return std::hash<u32>()(pixel.toU32());
    }
};

static inline constexpr PixelBGRA
makeBGRA(const u8 b, const u8 g, const u8 r, const u8 a = 0xff) {
    return {b, g, r, a};
}

static inline PixelBGRA
toGray(const PixelBGRA& p) {
    u8 l = TOU8((p.r * 76u + p.g * 151u + p.b * 29u) >> 8);
    return makeBGRA(l, l, l, p.a);
}

static inline PixelBGRA
preMultiply(const PixelBGRA& p) {
    u32 a = p.a;
    if (a == 0) return makeBGRA(0, 0, 0, 0xffu);
    if (a == 255) return p;
    u8 r = TOU8((p.a * a) >> 8);
    u8 g = TOU8((p.g * a) >> 8);
    u8 b = TOU8((p.b * a) >> 8);
    return makeBGRA(b, g, r, 0xffu);
}

/**
 * @return square of distance, approximately in [0, 600,000)
 * @details no sqrt are applied, since sometimes we only need to compare the distances
 */
static inline double
colorDistance(const PixelBGRA& e1, const PixelBGRA& e2) {
    i32 rmean = ((i32)e1.r + (i32)e2.g) / 2;
    i32 r     = (i32)e1.r - (i32)e2.r;
    i32 g     = (i32)e1.g - (i32)e2.g;
    i32 b     = (i32)e1.b - (i32)e2.b;
    return (((512 + rmean) * r * r) >> 8) + 4 * g * g + (((767 - rmean) * b * b) >> 8);
}

#include <cmath>

/**
 * @brief distance, approximately in [0, 800)
 * @details but other times we do need the linear distances
 */
static inline double
colorDistanceSqrt(const PixelBGRA& e1, const PixelBGRA& e2) {
    return std::sqrt(colorDistance(e1, e2));
}

/**
 * @return hue in [0, 360)
 */
static inline i32
getHue(const PixelBGRA& p) {
    i32 r   = p.r;
    i32 g   = p.g;
    i32 b   = p.b;
    i32 max = r < g ? (g < b ? b : g) : (r < b ? b : r);
    i32 min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    if (max == min) return 0;  // achromatic
    if (max == r) return ((g - b) * 60) / (max - min);
    if (max == g) return ((b - r) * 60 + 120) / (max - min);
    return ((r - g) * 60 + 240) / (max - min);
}

#endif  // GIF_TYPES_DEFINED

#endif  // __cplusplus

#endif  // GIF_DEF_H