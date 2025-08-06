/**
 * @file quant_native.cpp
 * @author Uyanide (uyanide@uyani.de)
 * @brief A complicated implementation of the not so complicated octree quantization algorithm.
 * @date 2025-06-16
 *
 * @copyright Copyright (c) 2025 Uyanide
 *
 */

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <numeric>
#include <queue>
#include <span>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#ifdef IMSQ_DEBUG
#include <chrono>
#endif  // IMSQ_DEBUG

#include "def.h"
#include "log.h"
#include "quantizer.h"

// just some random thresholds without even testing :)
//
// when the frame contains more pixels than this, downsample will be applied
static constexpr uint32_t DOWNSAMPLE_THRESHOLD = 1e6;
// when each block contains fewer pixels than this,
// an average color will be caculated for each block.
// otherwise, the color of the first pixel in each block will be used
static constexpr uint32_t DOWNSAMPLE_AVG_THRESHOLD = 100;

// Gray code mapping, useful when finding the closest color.
// 000 0 -> 0
// 001 1 -> 1
// 011 3 -> 2
// 010 2 -> 3
// 110 6 -> 4
// 111 7 -> 5
// 101 5 -> 6
// 100 4 -> 7
static constexpr uint8_t GRAY_MAP[8] = {
    0,
    1,
    3,
    2,
    7,
    6,
    4,
    5,
};
// and of course, the reverse version
static constexpr uint8_t REVERSE_GRAY_MAP[8] = {
    0,
    1,
    3,
    2,
    6,
    7,
    5,
    4,
};

// threshold for floydSteinbergDither
static constexpr int32_t FLOYD_MAX_ERROR = 256;

static constexpr double ORDERED_PENALTY_THRESHOLD = 0.000;

#ifdef IMSQ_DEBUG
static uint32_t _BEEP_CNT = 0;
#endif  // IMSQ_DEBUG

class OctreeQuantizer {
    struct Node {
        union {
            uint64_t sum[4];  // sum of r, g, b
            uint32_t childAdrs[8];
        } u{};  // since a node can not be both leaf and non-leaf

        uint32_t count = 0;
#ifdef IMSQ_DEBUG
        uint32_t parentAdr = 0;
        uint32_t selfAdr   = 0;
#endif  // IMSQ_DEBUG
        uint8_t childCnt = 0;
        uint8_t level    = 0;  // 1 to 8, 0 for root
        bool isLeaf      = false;
    };

    // indexing from MSB to LSB
    static constexpr std::array<uint8_t, 8>
    _octIndex(const PixelBGRA& c) noexcept {
        // definitly not AI generated ;D
        return {
            {
                GRAY_MAP[((c.r & 0b10000000) >> 5) | ((c.g & 0b10000000) >> 6) | ((c.b & 0b10000000) >> 7)],
                GRAY_MAP[((c.r & 0b01000000) >> 4) | ((c.g & 0b01000000) >> 5) | ((c.b & 0b01000000) >> 6)],
                GRAY_MAP[((c.r & 0b00100000) >> 3) | ((c.g & 0b00100000) >> 4) | ((c.b & 0b00100000) >> 5)],
                GRAY_MAP[((c.r & 0b00010000) >> 2) | ((c.g & 0b00010000) >> 3) | ((c.b & 0b00010000) >> 4)],
                GRAY_MAP[((c.r & 0b00001000) >> 1) | ((c.g & 0b00001000) >> 2) | ((c.b & 0b00001000) >> 3)],
                GRAY_MAP[((c.r & 0b00000100) << 0) | ((c.g & 0b00000100) >> 1) | ((c.b & 0b00000100) >> 2)],
                GRAY_MAP[((c.r & 0b00000010) << 1) | ((c.g & 0b00000010) << 0) | ((c.b & 0b00000010) >> 1)],
                GRAY_MAP[((c.r & 0b00000001) << 2) | ((c.g & 0b00000001) << 1) | ((c.b & 0b00000001) << 0)],
            }};
    }

    uint32_t
    _allocNode(const uint8_t level) noexcept {
        m_nodes.emplace_back(Node{.level = level});
        const auto idx = static_cast<uint32_t>(m_nodes.size() - 1);
        m_levels[level].emplace_back(idx);
        return idx;
    }

    static constexpr uint32_t
    _getDistance(const uint32_t a, const uint32_t b) {
        if (a > b) {
            return _getDistance(b, a);
        } else if (a == b) {
            return 0;
        } else {
            return std::min(b - a, a + 8 - b);
        }
    }

    void
    _reduce(std::vector<uint32_t>& finalNodeAdrs) noexcept {
        // Reduce the octree by merging nodes from bottom to top, level by level.
        auto diff = m_levels[8].size() - m_numColors;
        for (int level = 7; level >= 0 && diff > 0; --level) {
            auto& levelAdrs = m_levels[level];
            // index sort
            auto indexes = std::vector<uint32_t>(levelAdrs.size());
            std::iota(indexes.begin(), indexes.end(), 0);
            std::sort(indexes.begin(),
                      indexes.end(),
                      [this, &levelAdrs](uint32_t a, uint32_t b) {
                          return m_nodes[levelAdrs[a]].count < m_nodes[levelAdrs[b]].count;
                      });
            size_t size = indexes.size();
            for (size_t i = 0; i < size && diff > 0; ++i) {
                auto& node = m_nodes[levelAdrs[indexes[i]]];
                assert(node.childCnt > 0);  // won't happen anyways
                // case not all children should be merged
                if (node.childCnt > diff && size_t(node.childCnt) - 1 > diff) {
                    std::priority_queue<
                        std::pair<uint32_t, uint32_t>,
                        std::vector<std::pair<uint32_t, uint32_t>>,
                        std::greater<std::pair<uint32_t, uint32_t>>>
                        pq;  // min-heap
                    for (uint32_t idx = 0; idx < 8; ++idx) {
                        auto& childAdr = node.u.childAdrs[idx];
                        if (childAdr) {
                            const auto& childNode = m_nodes[childAdr];
                            pq.push({childNode.count, idx});
                        }
                    }
                    for (size_t i = 0; i < diff && !pq.empty(); ++i) {
                        const auto [count, idx] = pq.top();
                        pq.pop();
                        // merge into closest neighbor
                        Node* other = nullptr;
                        for (uint32_t offset = 1; !other && offset <= 4; ++offset) {
                            uint32_t post = (idx + offset) & 7;
                            uint32_t prev = (idx + 8 - offset) & 7;
                            if (node.u.childAdrs[post])
                                other = &m_nodes[node.u.childAdrs[post]];
                            else if (node.u.childAdrs[prev])
                                other = &m_nodes[node.u.childAdrs[prev]];
                        }
                        // should never happen
                        assert(other != nullptr);  // should be at least 2 children left
                        auto& childNode = m_nodes[node.u.childAdrs[idx]];
                        other->count += childNode.count;
                        other->u.sum[0] += childNode.u.sum[0];
                        other->u.sum[1] += childNode.u.sum[1];
                        other->u.sum[2] += childNode.u.sum[2];
                        node.u.childAdrs[idx] = 0;  // remove child
                        --(node.childCnt);
                    }
                    while (!pq.empty()) {
                        const auto [_, idx] = pq.top();
                        finalNodeAdrs.emplace_back(node.u.childAdrs[idx]);
                        pq.pop();
                    }
                    diff = 0;
                }
                // merge all children to parent
                else {
                    diff -= (node.childCnt - 1);
                    // inconvience of union, cannot directly add to sums
                    uint64_t sumR = 0, sumG = 0, sumB = 0;
                    for (auto childIdx : node.u.childAdrs) {
                        if (childIdx) {
                            sumR += m_nodes[childIdx].u.sum[0];
                            sumG += m_nodes[childIdx].u.sum[1];
                            sumB += m_nodes[childIdx].u.sum[2];
                        }
                    }
                    node.isLeaf   = true;
                    node.u.sum[0] = sumR;
                    node.u.sum[1] = sumG;
                    node.u.sum[2] = sumB;
                    node.childCnt = 0;
                    // case finished, insert this leaf node to result
                    if (!diff) finalNodeAdrs.emplace_back(levelAdrs[indexes[i]]);
                }
                if (!diff) {
                    finalNodeAdrs.reserve(m_numColors);
                    // add previous nodes of same level to result
                    for (size_t j = 0; j < i; ++j) {
                        finalNodeAdrs.emplace_back(levelAdrs[indexes[j]]);
                    }
                    // add child nodes of rest nodes of same level to result
                    for (size_t j = i + 1; j < size; ++j) {
                        for (const auto& childIdx :
                             m_nodes[levelAdrs[indexes[j]]].u.childAdrs) {
                            if (childIdx) {
                                finalNodeAdrs.emplace_back(childIdx);
                            }
                        }
                    }
                }
            }
        }
    }

    // get index in m_nodes, return 0 when failed
    uint32_t
    _getAddress(const PixelBGRA& color) const noexcept {
        if (!isFinished) return 0;
        auto indexes = _octIndex(color);

        uint32_t nodeAdr = 0;
        for (int i = 0; i <= 8; ++i) {
            const auto& node = m_nodes[nodeAdr];
            if (node.isLeaf) {
                return nodeAdr;
            }
            assert(i < 8);  // nodes with level 8 should definitely be leaf nodes
            if (i == 8) {
                GeneralLogger::error("Failed to lookup color while quantizing: dead end at last");
                return 0;  // dead end
            }
            const auto index = indexes[i];
            auto childAdr    = node.u.childAdrs[index];
            // case no such child
            if (!childAdr) {
                // actually only few pixels could reach here, around 10%, likely 1% or even 0%,
                // and most of them only happen when applying error-diffusion dithering
#ifdef IMSQ_DEBUG
                _BEEP_CNT++;
#endif  // IMSQ_DEBUG
                return 0;
            }
            nodeAdr = childAdr;
        }
        // unreachable
        assert(0);  // no color found
        GeneralLogger::error("Failed to lookup color while quantizing: no end");
        return 0;
    }

  public:
    OctreeQuantizer() = delete;

    OctreeQuantizer(const OctreeQuantizer&) = delete;

    OctreeQuantizer(uint32_t numColors, bool grayscale) noexcept
        : m_numColors(numColors), m_gray(grayscale) {
        _allocNode(0);  // root
    }

    bool
    isGrayScale() const noexcept {
        return m_gray;
    }

    bool
    isValid() const noexcept {
        return m_isValid;
    }

    std::vector<PixelBGRA>
    getPalette() const noexcept {
        if (!isFinished) return {};
        return m_palette;
    }

    void
    addColor(const PixelBGRA& color) noexcept {
        if (isFinished) return;
        assert(!m_gray || (color.r == color.g && color.g == color.b));  // grayscale colors only
        m_isValid    = true;
        auto indexes = _octIndex(color);
        auto nodeAdr = 0;
        for (int i = 0; i < 8; ++i) {
            // get index
            const auto index = indexes[i];
            // if no child node, allocate a new one
            auto childAdr = m_nodes[nodeAdr].u.childAdrs[index];
            if (!childAdr) {
                // ATTENTION: _allocNode() will push a new node to m_nodes,
                //            which could invalidate the references to items in m_nodes
                childAdr = m_nodes[nodeAdr].u.childAdrs[index] = _allocNode(i + 1);
                m_nodes[nodeAdr].childCnt += 1;
#ifdef IMSQ_DEBUG
                m_nodes[childAdr].parentAdr = nodeAdr;
                m_nodes[childAdr].selfAdr   = childAdr;
#endif  // IMSQ_DEBUG
            }
            ++(m_nodes[nodeAdr].count);
            // move to next level
            nodeAdr = m_nodes[nodeAdr].u.childAdrs[index];
        }
        auto& node = m_nodes[nodeAdr];
        node.u.sum[0] += color.r;
        node.u.sum[1] += color.g;
        node.u.sum[2] += color.b;
        node.count += 1;
        node.isLeaf = true;  // mark as leaf
    }

    std::vector<PixelBGRA>
    buildPalette() noexcept {
        if (isFinished) return {};
        std::vector<uint32_t> finalNodeAdrs;
        if (m_levels[8].size() > m_numColors) {
            _reduce(finalNodeAdrs);
        }
        // case not enough colors, just use all colors
        else {
            finalNodeAdrs.resize(m_levels[8].size());
            std::memcpy(finalNodeAdrs.data(),
                        m_levels[8].data(),
                        m_levels[8].size() * sizeof(uint32_t));
        }
        // sort the palette according to grayscale values
        std::vector<PixelBGRA> colors(finalNodeAdrs.size());
        // first: grayscale, second: index in finalNodeAdrs
        std::vector<std::pair<uint32_t, uint32_t>> grays(finalNodeAdrs.size());
        m_palette.reserve(finalNodeAdrs.size());
        uint32_t idx = 0;
        for (const auto adr : finalNodeAdrs) {
            const auto& node = m_nodes[adr];
            // calculate the average color
            const uint8_t r = TOU8C(node.u.sum[0] / node.count);
            const uint8_t g = TOU8C(node.u.sum[1] / node.count);
            const uint8_t b = TOU8C(node.u.sum[2] / node.count);

            colors[idx] = makeBGRA(b, g, r);
            grays[idx]  = {toGray(colors[idx]).r, idx};
            ++idx;
        }
        // white to black, since the possible following paddings are black
        std::sort(grays.begin(), grays.end(), [](const auto& a, const auto& b) {
            return a.first > b.first;
        });
        idx = 0;
        for (const auto gray : grays) {
            m_colorMap[finalNodeAdrs[gray.second]] = idx++;
            m_palette.push_back(colors[gray.second]);
        }
        m_palette.resize(m_numColors);
        for (uint32_t i = grays.size(); i < m_numColors; ++i) {
            m_palette[i] = makeBGRA(0, 0, 0);  // fill with black
        }
        isFinished = true;
        return m_palette;
    }

    // get index in m_palette
    uint32_t
    getPaletteIndex(const PixelBGRA& color) const noexcept {
        if (!isFinished) return 0;
        const auto address  = _getAddress(color);
        const uint8_t index = address ? m_colorMap.at(address) : findClosestColor(color);
        return index;
        // slower, and worse quality:
        // return findClosestColorOct(color);
    }

    // make use of properties of octree to find the closest color
    uint32_t
    findClosestColorOct(const PixelBGRA& color) const noexcept {
        if (!isFinished) return 0;
        auto indexes = _octIndex(color);

        uint32_t nodeAdr = 0;
        for (int i = 0; i <= 8; ++i) {
            const auto& node = m_nodes[nodeAdr];
            if (node.isLeaf) {
                assert(m_colorMap.find(nodeAdr) != m_colorMap.end());  // leaf nodes should always be in m_colorMap
                const auto it = m_colorMap.find(nodeAdr);
                if (it == m_colorMap.end()) {
                    GeneralLogger::error("Failed to lookup color while quantizing: no color found in map");
                    return 0;  // no color found
                }
                return it->second;  // found color, return index in m_palette
            }
            assert(i < 8);  // nodes with level 8 should definitely be leaf nodes
            if (i == 8) {
                GeneralLogger::error("Failed to lookup color while quantizing: dead end (final level)");
                return 0;  // dead end
            }
            const auto index = indexes[i];
            auto childAdr    = node.u.childAdrs[index];
            // case no such child
            if (!childAdr) {
#ifdef IMSQ_DEBUG
                _BEEP_CNT++;
#endif
                // dead end, actually not possible
                assert(node.childCnt > 0);  // non-leaf nodes should have at least one child
                if (!node.childCnt) {
                    GeneralLogger::error("Failed to lookup color while quantizing: dead end (no children)");
                    return 0;
                }
                // single child, just go to it
                else if (node.childCnt == 1) {
                    for (uint32_t child = 0; child < 8; ++child) {
                        if (node.u.childAdrs[child]) {
                            childAdr = node.u.childAdrs[child];
                            break;
                        }
                    }
                }
                // more than 1 children, find the closest one
                else {
                    for (uint32_t offset = 1; offset <= 4; ++offset) {
                        const uint32_t l = (index + 8 - offset) & 7, r = (index + offset) & 7;
                        if (node.u.childAdrs[l])
                            childAdr = node.u.childAdrs[l];
                        else if (node.u.childAdrs[r]) {
                            childAdr = node.u.childAdrs[r];
                        }
                    }
                    assert(childAdr != 0);
                    if (!childAdr) {
                        GeneralLogger::error("Failed to lookup color while quantizing: dead end (no alternative)");
                        return 0;
                    }
                }
            }
            nodeAdr = childAdr;
        }
        // unreachable
        assert(0);  // no color found
        GeneralLogger::error("Failed to lookup color while quantizing: no end");
        return 0;
    }

    // how about just using the stupid but straightforward way?
    uint32_t
    findClosestColor(const PixelBGRA& color) const noexcept {
        if (!isFinished) return 0;
        uint32_t ret   = 0;
        double minDist = -1;
        std::function<double(const PixelBGRA&, const PixelBGRA&)> calDist;
        if (m_gray) {
            calDist = [](const PixelBGRA& a, const PixelBGRA& b) {
                return std::abs(toGray(a).r - toGray(b).r);
            };
        } else {
            calDist = colorDistance;
        }
        for (uint32_t i = 0; i < m_palette.size(); ++i) {
            const auto& paletteColor = m_palette[i];
            if (paletteColor == color) {
                return i;
            }
            const auto dist = calDist(color, paletteColor);
            if (minDist == -1 || dist < minDist) {
                minDist = dist;
                ret     = i;
            }
        }
        return ret;
    }

  private:
    uint32_t m_numColors;
    bool isFinished = false;
    bool m_isValid  = false;  // will be true if any color is added
    bool m_gray     = false;

    std::vector<Node> m_nodes;
    std::array<std::vector<uint32_t>, 9> m_levels;
    std::vector<PixelBGRA> m_palette;
    std::unordered_map<uint32_t, uint8_t> m_colorMap;  // idx in m_nodes to idx in m_palette
    // std::unordered_map<PixelBGRA, uint8_t, PixelRGBAHash> m_colorCache; // useless
};

static std::pair<uint32_t, uint32_t>
downsampleStep(uint32_t width, uint32_t height, uint32_t maxPixels) noexcept {
    if (width * height <= DOWNSAMPLE_THRESHOLD) {
        return {1, 1};  // no downsampling needed
    }
    uint32_t step  = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(width) * height / maxPixels)));
    uint32_t stepW = step, stepH = step;
    // case step is too large, e.g. processing image with dimension of 1000000x1
    if (step >= width / 2 && step >= height / 2) {
        stepW = width;
        stepH = height;
    } else if (step >= width / 2) {
        stepW = width;
        stepH = static_cast<uint32_t>(std::ceil(static_cast<double>(height) / maxPixels));
    } else if (step >= height / 2) {
        stepW = static_cast<uint32_t>(std::ceil(static_cast<double>(width) / maxPixels));
        stepH = height;
    }
    return {stepW, stepH};
}

// floyd-steinberg dithering
static void
floydSteinbergDithering(std::vector<PixelBGRA>& pixels,  // This is pixelCpy
                        const std::span<PixelBGRA>& palette,
                        std::vector<uint8_t>& indices,
                        const uint32_t width,
                        const uint32_t height,
                        const OctreeQuantizer& quantizer,
                        const bool hasTransparency,
                        const uint8_t transparentThreshold) noexcept {
    // Helper lambda to apply error to a channel, ensuring clamping
    auto apply_error_to_channel = [](uint8_t& v, const int32_t e, const int32_t f) {
        const int32_t ret = v + e * f / 16;
        v                 = ret < 0 ? 0 : ret > 255 ? 255
                                                    : static_cast<uint8_t>(ret);
    };

    std::fill(indices.begin(), indices.end(), uint8_t(0));
    if (hasTransparency) {
        for (uint32_t i = 0; i < pixels.size(); ++i) {
            if (pixels[i].a <= transparentThreshold) {
                indices[i] = palette.size();
            }
        }
    }

    for (uint32_t y = 0, pixIndex = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++, pixIndex++) {
            // Skip transparent pixels
            if (indices[pixIndex]) {
                continue;
            }

            const PixelBGRA& orgColor = pixels[pixIndex];
            uint8_t& pltIndex         = indices[pixIndex];
            pltIndex                  = quantizer.getPaletteIndex(orgColor);

            const PixelBGRA& pltColor = palette[pltIndex];

            // case exactly the same color
            if (orgColor.r == pltColor.r && orgColor.g == pltColor.g && orgColor.b == pltColor.b) {
                continue;
            }

            const int32_t rQuantError = std::clamp(
                static_cast<int32_t>(orgColor.r) - static_cast<int32_t>(pltColor.r),
                -FLOYD_MAX_ERROR,
                FLOYD_MAX_ERROR);
            const int32_t gQuantError = std::clamp(
                static_cast<int32_t>(orgColor.g) - static_cast<int32_t>(pltColor.g),
                -FLOYD_MAX_ERROR,
                FLOYD_MAX_ERROR);
            const int32_t bQuantError = std::clamp(
                static_cast<int32_t>(orgColor.b) - static_cast<int32_t>(pltColor.b),
                -FLOYD_MAX_ERROR,
                FLOYD_MAX_ERROR);
            // Right pixel
            if (x + 1 < width) {
                PixelBGRA& nextPixel = pixels[pixIndex + 1];
                if (!indices[pixIndex + 1]) {
                    apply_error_to_channel(nextPixel.r, rQuantError, 7);
                    apply_error_to_channel(nextPixel.g, gQuantError, 7);
                    apply_error_to_channel(nextPixel.b, bQuantError, 7);
                }
            }
            if (y + 1 < height) {
                // Bottom-left pixel
                if (x > 0) {
                    PixelBGRA& prevPixel = pixels[pixIndex + width - 1];
                    if (!indices[pixIndex + width - 1]) {
                        apply_error_to_channel(prevPixel.r, rQuantError, 3);
                        apply_error_to_channel(prevPixel.g, gQuantError, 3);
                        apply_error_to_channel(prevPixel.b, bQuantError, 3);
                    }
                }
                // Bottom pixel
                PixelBGRA& belowPixel = pixels[pixIndex + width];
                if (!indices[pixIndex + width]) {
                    apply_error_to_channel(belowPixel.r, rQuantError, 5);
                    apply_error_to_channel(belowPixel.g, gQuantError, 5);
                    apply_error_to_channel(belowPixel.b, bQuantError, 5);
                }
                // Bottom-right pixel
                if (x + 1 < width) {
                    PixelBGRA& belowNextPixel = pixels[pixIndex + width + 1];
                    if (!indices[pixIndex + width + 1]) {
                        apply_error_to_channel(belowNextPixel.r, rQuantError, 1);
                        apply_error_to_channel(belowNextPixel.g, gQuantError, 1);
                        apply_error_to_channel(belowNextPixel.b, bQuantError, 1);
                    }
                }
            }
        }
    }
}

// ordered dithering with multiple colors
static void
orderedDithering(std::vector<PixelBGRA>& pixels,  // This is pixelCpy
                 const std::span<PixelBGRA>& palette,
                 std::vector<uint8_t>& indices,
                 const uint32_t width,
                 const uint32_t height,
                 const OctreeQuantizer& quantizer,
                 const bool hasTransparency,
                 const uint8_t transparentThreshold,
                 const uint32_t threadCount = 1) noexcept {

    static constexpr int32_t BayerMat[4][4] = {{0, 8, 2, 10},
                                               {12, 4, 14, 6},
                                               {3, 11, 1, 9},
                                               {15, 7, 13, 5}};
    std::fill(indices.begin(), indices.end(), uint8_t(0));
    if (hasTransparency) {
        for (uint32_t i = 0; i < pixels.size(); ++i) {
            if (pixels[i].a <= transparentThreshold) {
                indices[i] = palette.size();
            }
        }
    }

    static uint32_t MIN_CHUNK_SIZE = 1024 * 1024;  // 1MB
    static auto getChunkSize =
        [](uint32_t bytesLen, uint32_t threadCount) {
            uint32_t size = ((bytesLen >> 2) / threadCount) << 2;
            size          = size < MIN_CHUNK_SIZE ? MIN_CHUNK_SIZE : size;
            threadCount   = (bytesLen + size - 1) / size;
            return size;
        };

    const auto multiThreadProc =
        [&threadCount, &indices](const std::function<void(size_t, size_t)>& worker) {
            size_t chunkSize = getChunkSize(indices.size(), threadCount);
            std::vector<std::thread> threads;
            for (uint32_t i = 0; i < threadCount; ++i) {
                size_t start = i * chunkSize;
                size_t end   = std::min((i + 1) * chunkSize, indices.size());
                threads.emplace_back(worker, start, end);
            }
            for (auto& thread : threads) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
        };

    if (quantizer.isGrayScale()) {
        const auto worker = [&indices, &palette, &pixels, &quantizer, width](const size_t start, const size_t end) {
            for (size_t pixIndex = start; pixIndex < end; ++pixIndex) {
                // Skip transparent pixels
                if (indices[pixIndex]) continue;

                const uint32_t y = pixIndex / width, x = pixIndex % width;

                const uint32_t orgColor = pixels[pixIndex].r;
                uint8_t& pltIndex       = indices[pixIndex];
                pltIndex                = quantizer.getPaletteIndex(pixels[pixIndex]);
                const uint32_t pltColor = palette[pltIndex].r;

                // case exactly the same color
                if (orgColor == pltColor) continue;
                // should be brightened
                if (orgColor > pltColor) {
                    // case already the brightest
                    if (pltIndex == 0) continue;
                    const uint8_t prevColor = palette[pltIndex - 1].r;
                    if (((orgColor - pltColor) << 4) > (prevColor - pltColor) * BayerMat[y & 3][x & 3]) {
                        // use the previous color
                        pltIndex--;
                    }
                }
                // should be darkened
                else {
                    // case already the darkest
                    if (pltIndex == palette.size() - 1) continue;
                    const uint8_t nextColor = palette[pltIndex + 1].r;
                    if (((pltColor - orgColor) << 4) > (pltColor - nextColor) * BayerMat[y & 3][x & 3]) {
                        // use the next color
                        pltIndex++;
                    }
                }
            }
        };

        size_t chunkSize = getChunkSize(indices.size(), threadCount);
        std::vector<std::thread> threads;
        for (uint32_t i = 0; i < threadCount; ++i) {
            size_t start = i * chunkSize;
            size_t end   = std::min((i + 1) * chunkSize, indices.size());
            threads.emplace_back(worker, start, end);
        }
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        multiThreadProc(worker);
    } else {
        // Ref: https://bisqwit.iki.fi/story/howto/dither/jy/
        // modified from Yliluoma's ordered dithering algorithm 1
        static const auto colorDistance = [](int32_t ar, int32_t ag, int32_t ab, int32_t br, int32_t bg, int32_t bb) {
            // precompute
            static constexpr double fr  = 0.299 / 255 / 255,
                                    fg  = 0.587 / 255 / 255,
                                    fb  = 0.114 / 255 / 255,
                                    lfr = 0.299 / 255,
                                    lfg = 0.587 / 255,
                                    lfb = 0.114 / 255;

            double diffL = (lfr * (ar - br) + lfg * (ag - bg) + lfb * (ab - bb));
            double diffR = fr * (ar - br) * (ar - br),
                   diffG = fg * (ag - bg) * (ag - bg),
                   diffB = fb * (ab - bb) * (ab - bb);
            return (diffR + diffG + diffB) * 0.75 + diffL * diffL;
        };

        const size_t pSize = palette.size();
        // precalculation
        std::vector<double> dists(pSize * pSize);
        for (size_t i = 0, start = 1; i < pSize; ++i, start += pSize + 1) {
            size_t idx = start;

            for (size_t j = i + 1; j < pSize; ++j, ++idx) {
                const auto &ci = palette[i], cj = palette[j];
                const int32_t ri = ci.r, gi = ci.g, bi = ci.b,
                              rj = cj.r, gj = cj.g, bj = cj.b;
                // precompute weighted distances
                dists[idx] = colorDistance(ri, gi, bi, rj, gj, bj) * 0.1;
            }
        }

        const auto makeMixingPlan =
            [pSize, &palette, &dists, &quantizer](
                const PixelBGRA& color,
                int32_t& finalRatio,
                size_t& index1,
                size_t& index2) {
                int32_t r = color.r, g = color.g, b = color.b;
                double leastPenalty = 1e99;
                size_t i            = quantizer.getPaletteIndex(color);
                for (size_t j = 0; j < pSize; ++j) {
                    const size_t pltIdx = i * pSize + j;
                    if (dists[pltIdx] / 2 >= leastPenalty) continue;
                    const auto& color1 = palette[i];
                    const auto& color2 = palette[j];
                    int32_t r1 = color1.r, g1 = color1.g, b1 = color1.b,
                            r2 = color2.r, g2 = color2.g, b2 = color2.b;
                    int32_t ratio = 8;
                    if (color1 != color2) {
                        ratio = std::clamp<int32_t>(
                            ((r1 == r2 ? 0 : (r - r1) * 299 * 16 / (r2 - r1)) +
                             (g1 == g2 ? 0 : (g - g1) * 587 * 16 / (g2 - g1)) +
                             (b1 == b2 ? 0 : (b - b1) * 114 * 16 / (b2 - b1))) /
                                ((r1 == r2 ? 0 : 299) +
                                 (g1 == g2 ? 0 : 587) +
                                 (b1 == b2 ? 0 : 114)),
                            0,
                            16);
                    }
                    int32_t r0     = r1 + ratio * (r2 - r1) / 16,
                            g0     = g1 + ratio * (g2 - g1) / 16,
                            b0     = b1 + ratio * (b2 - b1) / 16;
                    double penalty = colorDistance(r, g, b, r0, g0, b0) +
                                     dists[pltIdx] * (fabs(ratio / 16. - 0.5) + 0.5);
                    if (penalty < leastPenalty) {
                        leastPenalty = penalty;
                        index1       = i;
                        index2       = j;
                        finalRatio   = ratio;
                    }
                    if (leastPenalty < ORDERED_PENALTY_THRESHOLD) return;
                }
            };

        const auto proc =
            [&indices, &pixels, &makeMixingPlan, width](
                const size_t start,
                const size_t end) {
                for (size_t pixIdx = start; pixIdx < end; ++pixIdx) {
                    if (indices[pixIdx]) continue;
                    int32_t ratio = 8;
                    size_t index1 = 0, index2 = 0;
                    makeMixingPlan(pixels[pixIdx], ratio, index1, index2);
                    indices[pixIdx] = BayerMat[(pixIdx / width) & 3][(pixIdx % width) & 3] < ratio ? index2 : index1;
                }
            };

        const auto worker = [&proc](const size_t start, const size_t end) {
            proc(start, end);
        };

        multiThreadProc(worker);
    }
}

// #include <chrono>

GIFImage::QuantizerResult
GIFImage::quantize(const std::span<PixelBGRA>& pixels,
                   const uint32_t width,
                   const uint32_t height,
                   const uint32_t numColors,
                   const GIFImage::DitherMode ditherMode,
                   const bool grayScale,
                   const bool transparency,
                   const uint8_t transparentThreshold,
                   const bool downsample) noexcept {
    // parameter validation
    bool doExit = false;
    if (numColors < 1 || numColors > 256) {
        GeneralLogger::error("numColors out of range: " + std::to_string(numColors) +
                             ", must be between 2 and 256");
        doExit = true;
    }
    if (numColors == 256 && transparency) {
        GeneralLogger::error("Transparency is not supported with 256 colors");
        doExit = true;
    }
    if (width == 0 || height == 0) {
        GeneralLogger::error("Image dimensions must be greater than 0");
        doExit = true;
    }
    if (pixels.size() != width * height) {
        GeneralLogger::error("Pixel data size does not match image dimensions: " +
                             std::to_string(pixels.size()) + " != " + std::to_string(width * height));
        doExit = true;
    }
    if (pixels.size() >= UINT_MAX) {
        GeneralLogger::error("Pixel data size exceeds maximum limit: " + std::to_string(pixels.size()));
        doExit = true;
    }
    if (doExit) {
        return QuantizerResult{
            .isValid      = false,
            .errorMessage = "Invalid parameters for quantization",
        };
    }

#ifdef IMSQ_DEBUG
    GeneralLogger::info("Starting quantization");
    auto start = std::chrono::high_resolution_clock::now();
#endif  // IMSQ_DEBUG
    OctreeQuantizer quantizer(numColors, grayScale);
    QuantizerResult result{
        .hasTransparency   = transparency,
        .width             = width,
        .height            = height,
        .transparencyIndex = transparency ? numColors : 0,
    };
#ifdef IMSQ_DEBUG
    auto now = std::chrono::high_resolution_clock::now();
    printf("%lld ms\n",
           std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
    start = now;
#endif  // IMSQ_DEBUG

    // add pixels to quantizer
    auto [stepW, stepH] = downsampleStep(width, height, DOWNSAMPLE_THRESHOLD);
#ifdef IMSQ_DEBUG
    GeneralLogger::info("Downsampling steps: " + std::to_string(stepW) + "x" + std::to_string(stepH));
    GeneralLogger::info("Adding pixels to quantizer");
#endif
    if (!downsample || (stepW == 1 && stepH == 1)) {
        for (const auto& pixel : pixels) {
            // skip transparent pixels
            if (transparency && pixel.a <= transparentThreshold) {
                continue;
            }
            quantizer.addColor(grayScale ? toGray(pixel) : pixel);
        }
    } else if (stepW * stepH < DOWNSAMPLE_AVG_THRESHOLD) {
        for (uint32_t y = 0; y < height; y += stepH) {
            for (uint32_t x = 0; x < width; x += stepW) {
                uint32_t r = 0, g = 0, b = 0, count = 0;
                for (uint32_t yy = y; yy < std::min(y + stepH, height); ++yy) {
                    for (uint32_t xx = x; xx < std::min(x + stepW, width); ++xx) {
                        const auto& pixel = pixels[yy * width + xx];
                        // skip transparent pixels
                        if (transparency && pixel.a <= transparentThreshold) {
                            continue;
                        }
                        r += pixel.r;
                        g += pixel.g;
                        b += pixel.b;
                        ++count;
                    }
                }
                if (count > 0) {
                    const auto c = makeBGRA(TOU8C(b / count), TOU8C(g / count), TOU8C(r / count));
                    quantizer.addColor(grayScale ? toGray(c) : c);
                }
            }
        }
    } else {
        for (uint32_t y = 0; y < height; y += stepH) {
            for (uint32_t x = 0; x < width; x += stepW) {
                const auto& pixel = pixels[y * width + x];
                // skip transparent pixels
                if (transparency && pixel.a <= transparentThreshold) {
                    continue;
                }
                quantizer.addColor(grayScale ? toGray(pixel) : pixel);
            }
        }
    }
#ifdef IMSQ_DEBUG
    now = std::chrono::high_resolution_clock::now();
    printf("%lld ms\n",
           std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
    start = now;
    GeneralLogger::info("Building palette");
#endif  // IMSQ_DEBUG

    // If no pixels were added
    if (!quantizer.isValid()) {
        GeneralLogger::warn("All pixels are considered as transparent, skipping quantization.");
        return QuantizerResult{
            .isValid      = false,
            .errorMessage = "All pixels are transparent",
        };
    }

    result.palette = quantizer.buildPalette();
#ifdef IMSQ_DEBUG
    now = std::chrono::high_resolution_clock::now();
    printf("%lld ms\n",
           std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
    start = now;
    GeneralLogger::info("Querying color indices");
#endif  // IMSQ_DEBUG
    result.indices.reserve(pixels.size());
    if (ditherMode == DitherMode::DitherFloydSteinberg) {
        // no need to query colors here if any error-diffusion dithering is enabled
        result.indices.resize(pixels.size());
    } else {
        for (const auto& pixel : pixels) {
            // skip transparent pixels
            if (transparency && pixel.a <= transparentThreshold) {
                result.indices.push_back(result.transparencyIndex);
                continue;
            }
            result.indices.push_back(quantizer.getPaletteIndex(grayScale ? toGray(pixel) : pixel));
        }
    }
#ifdef IMSQ_DEBUG
    now = std::chrono::high_resolution_clock::now();
    printf("%lld ms\n",
           std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
    start = now;

    GeneralLogger::info("Applying dithering");
#endif  // IMSQ_DEBUG
    // apply dithering if needed
    if (ditherMode == DitherMode::DitherFloydSteinberg) {
        auto pixelCpy = std::vector<PixelBGRA>(pixels.begin(), pixels.end());
        floydSteinbergDithering(
            pixelCpy,
            result.palette,
            result.indices,
            width,
            height,
            quantizer,
            transparency,
            transparentThreshold);
    } else if (ditherMode == DitherMode::DitherOrdered) {
        auto pixelCpy = std::vector<PixelBGRA>(pixels.begin(), pixels.end());
        orderedDithering(
            pixelCpy,
            result.palette,
            result.indices,
            width,
            height,
            quantizer,
            transparency,
            transparentThreshold);
    }
#ifdef IMSQ_DEBUG
    now = std::chrono::high_resolution_clock::now();
    printf("%lld ms\n",
           std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
    start = now;

    GeneralLogger::info("Quantization completed successfully");
    for (const auto& color : result.palette) {
        printf("#%02x%02x%02x\n", color.r, color.g, color.b);
    }
    printf("Beeped %d times, %.2f%%\n", _BEEP_CNT, static_cast<double>(_BEEP_CNT) / pixels.size() * 100.0);
#endif  // IMSQ_DEBUG
    result.isValid = true;
    return result;
}

std::optional<PixelBGRA>
GIFImage::findUnusedColor(const std::span<PixelBGRA>& pixels, const uint32_t step) noexcept {
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