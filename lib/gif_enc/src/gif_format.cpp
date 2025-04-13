#include "gif_format.h"

#include <string>
#include <vector>
using std::vector, std::string;

std::vector<uint8_t>
GIFEnc::gifHeader(const uint32_t width,
                  const uint32_t height,
                  const uint32_t backgroundIndex,
                  const uint32_t minCodeLength,
                  const uint32_t loops,
                  const bool hasGlobalColorTable,
                  const std::vector<PixelBGRA>& globalColorTable) noexcept {
    if (minCodeLength < 2 || minCodeLength > 8) {
        return {};
    }
    if (hasGlobalColorTable) {
        if (backgroundIndex >= globalColorTable.size()) {
            return {};
        }
        if (globalColorTable.size() > 1u << minCodeLength || globalColorTable.size() <= 1u << (minCodeLength - 1)) {
            return {};
        }
    }
    vector<uint8_t> ret{
        0x47,
        0x49,
        0x46,
        0x38,
        0x39,
        0x61,  // "GIF89a"
        TOU8(width & 0xFF),
        TOU8(width >> 8),
        TOU8(height & 0xFF),
        TOU8(height >> 8),
        TOU8((hasGlobalColorTable ? 0x80 : 0) | ((GIF_COLOR_RES - 1) << 4) | (minCodeLength - 1)),
        TOU8(backgroundIndex),
        0x00,
    };
    ret.reserve(ret.size() + ((1u << minCodeLength) * 3) + 19);
    if (hasGlobalColorTable) {
        for (uint32_t i = 0; i < (1u << minCodeLength); i++) {
            if (static_cast<size_t>(i) >= globalColorTable.size()) {
                ret.push_back(0);
                ret.push_back(0);
                ret.push_back(0);
            } else {
                ret.push_back(globalColorTable[i].r);
                ret.push_back(globalColorTable[i].g);
                ret.push_back(globalColorTable[i].b);
            }
        }
    }
    const vector<uint8_t> appExt{
        0x21,
        0xFF,
        0x0B,  // Application Extension
        0x4E,
        0x45,
        0x54,
        0x53,
        0x43,
        0x41,
        0x50,
        0x45,
        0x32,
        0x2E,
        0x30,  // "NETSCAPE2.0"
        0x03,
        0x01,
        TOU8(loops & 0xFF),
        TOU8(loops >> 8),
        0x00,
    };
    ret.insert(ret.end(), appExt.begin(), appExt.end());
    return ret;
}

std::vector<uint8_t>
GIFEnc::gifFrameHeader(uint32_t width,
                       uint32_t height,
                       uint32_t delay,
                       bool hasTransparency,
                       uint32_t transparentIndex,
                       uint32_t disposalMethod,
                       uint32_t minCodeLength,
                       const std::vector<PixelBGRA>& palette) noexcept {
    if (minCodeLength < 2 || minCodeLength > 8) {
        return {};
    }
    if (hasTransparency && (transparentIndex >= (1u << minCodeLength))) {
        return {};
    }
    if (!palette.empty()) {
        if (palette.size() > 1u << minCodeLength || palette.size() <= 1u << (minCodeLength - 1)) {
            return {};
        }
    }
    if (disposalMethod > 3) {
        return {};
    }
    delay /= 10;
    vector<uint8_t> ret{
        0x21,
        0xF9,
        0x04,  // Graphic Control Extension
        TOU8(hasTransparency ? 0x01u | (disposalMethod << 2) : 0x00u),
        TOU8(delay & 0xFFu),
        TOU8(delay >> 8),
        TOU8(hasTransparency ? transparentIndex : 0x00u),
        0x00,
        0x2C,  // Image Descriptor
        TOU8(0),
        TOU8(0),
        TOU8(0),
        TOU8(0),
        TOU8(width & 0xFFu),
        TOU8(width >> 8),
        TOU8(height & 0xFFu),
        TOU8(height >> 8),
        TOU8(0x00u | (palette.empty() ? 0u : (0x80u | (minCodeLength - 1)))),
    };
    ret.reserve(ret.size() + ((1u << minCodeLength) * 3));
    if (!palette.empty()) {
        for (const auto& color : palette) {
            ret.push_back(color.r);
            ret.push_back(color.g);
            ret.push_back(color.b);
        }
        for (size_t i = palette.size(); i < (1u << minCodeLength); i++) {
            ret.push_back(0);
            ret.push_back(0);
            ret.push_back(0);
        }
    }
    ret.push_back(TOU8(minCodeLength));
    return ret;
}

vector<uint8_t>
GIFEnc::gifApplicationExtension(const string& identifier,
                                const string& authentication,
                                const vector<uint8_t>& data) noexcept {
    if (identifier.size() != 8 || authentication.size() != 3) {
        return {};
    }
    const auto size = identifier.size() + authentication.size() + (data.size() + 254) / 255 + data.size() + 1;
    vector<uint8_t> ret{
        0x21,
        0xFF,
        0x0B,  // 11 bytes
    };
    ret.reserve(size);
    for (const auto c : identifier) {
        ret.push_back(static_cast<uint8_t>(c));
    }
    for (auto c : authentication) {
        ret.push_back(static_cast<uint8_t>(c));
    }
    if (data.empty()) ret.push_back(0);
    for (size_t i = 0; i < data.size(); i += 255) {
        auto chunkSize = std::min<size_t>(255, data.size() - i);
        ret.push_back(TOU8(chunkSize));
        ret.insert(ret.end(), data.begin() + i, data.begin() + i + chunkSize);
    }
    ret.push_back(0);
    return ret;
}