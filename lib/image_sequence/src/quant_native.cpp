/**
 * @file quant_native.cpp
 * @author Uyanide (uyanide@uyani.de)
 * @brief A complicated implementation of the not so complicated octree quantization algorithm.
 * @date 2025-06-16
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <algorithm>
#include <array>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <numeric>
#include <queue>
#include <span>
#include <unordered_set>

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
static constexpr int32_t FLOYD_MAX_ERROR = 24;

// static uint32_t _BEEP_CNT = 0;

class OctreeQuantizer {
    struct Node {
        union {
            uint64_t sum[4];  // sum of r, g, b
            uint32_t childIdxs[8];
        } u{};  // since a node can not be both leaf and non-leaf

        uint32_t count     = 0;
        uint32_t parentIdx = 0;
        bool isLeaf        = false;
        uint8_t childCnt   = 0;
        uint8_t level      = 0;  // 1 to 8, 0 for root

        void
        clearChildren() noexcept {
            if (!childCnt) return;
            childCnt = 0;
        }
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
    _reduce(std::vector<uint32_t>& finalNodePtrs) noexcept {
        // Reduce the octree by merging nodes from bottom to top, level by level.
        auto diff = m_levels[8].size() - m_numColors;
        for (int level = 7; level >= 0 && diff > 0; --level) {
            auto& levelIdxs = m_levels[level];
            auto indices    = std::vector<uint32_t>(levelIdxs.size());
            std::iota(indices.begin(), indices.end(), 0);
            std::sort(indices.begin(),
                      indices.end(),
                      [this, &levelIdxs](uint32_t a, uint32_t b) {
                          return m_nodes[levelIdxs[a]].count < m_nodes[levelIdxs[b]].count;
                      });
            size_t size = indices.size();
            for (size_t i = 0; i < size && diff > 0; ++i) {
                auto& node = m_nodes[levelIdxs[indices[i]]];
                assert(node.childCnt > 0);  // won't happen anyways
                // case not all children should be merged
                if (node.childCnt > diff && node.childCnt - 1 > diff) {
                    std::priority_queue<
                        std::pair<uint32_t, uint32_t>,
                        std::vector<std::pair<uint32_t, uint32_t>>,
                        std::greater<std::pair<uint32_t, uint32_t>>>
                        pq;  // min-heap
                    for (uint32_t idx = 0; idx < 8; ++idx) {
                        auto& childIdx = node.u.childIdxs[idx];
                        if (childIdx) {
                            const auto& childNode = m_nodes[childIdx];
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
                            if (node.u.childIdxs[post])
                                other = &m_nodes[node.u.childIdxs[post]];
                            else if (node.u.childIdxs[prev])
                                other = &m_nodes[node.u.childIdxs[prev]];
                        }
                        // should never happen
                        assert(other != nullptr);  // should be at least 2 children left
                        auto& childNode = m_nodes[node.u.childIdxs[idx]];
                        other->count += childNode.count;
                        other->u.sum[0] += childNode.u.sum[0];
                        other->u.sum[1] += childNode.u.sum[1];
                        other->u.sum[2] += childNode.u.sum[2];
                        node.u.childIdxs[idx] = 0;  // remove child
                        --(node.childCnt);
                    }
                    while (!pq.empty()) {
                        const auto [_, idx] = pq.top();
                        finalNodePtrs.emplace_back(node.u.childIdxs[idx]);
                        pq.pop();
                    }
                    diff = 0;
                } else {
                    diff -= (node.childCnt - 1);
                    // merge all children to parent
                    uint64_t sumR = 0, sumG = 0, sumB = 0;
                    for (auto childIdx : node.u.childIdxs) {
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
                    node.clearChildren();
                    if (!diff) finalNodePtrs.emplace_back(levelIdxs[indices[i]]);
                }
                if (!diff) {
                    finalNodePtrs.reserve(m_numColors);
                    // add previous nodes of same level to result
                    for (size_t j = 0; j < i; ++j) {
                        finalNodePtrs.emplace_back(levelIdxs[indices[j]]);
                    }
                    // add child nodes of rest nodes of same level to result
                    for (size_t j = i + 1; j < size; ++j) {
                        for (const auto& childIdx :
                             m_nodes[levelIdxs[indices[j]]].u.childIdxs) {
                            if (childIdx) {
                                finalNodePtrs.emplace_back(childIdx);
                            }
                        }
                    }
                }
            }
        }
    }

    // get index in m_nodes
    uint32_t
    _getIndex(const PixelBGRA& color) const noexcept {
        if (!isFinished) return 0;
        auto indexes     = _octIndex(color);
        uint32_t nodeIdx = 0;
        for (int i = 0; i <= 8; ++i) {
            const auto& node = m_nodes[nodeIdx];
            if (node.isLeaf) {
                return nodeIdx;
            }
            assert(i < 8);  // nodes with level 8 should definitely be leaf nodes
            if (i == 8) {
                GeneralLogger::error("Failed to lookup color while quantizing: dead end at last");
                return 0;  // dead end
            }
            const auto index = indexes[i];
            auto childIdx    = node.u.childIdxs[index];
            // case no such child
            if (!childIdx) {
                // // dead end, actually not possible
                // assert(node.childCnt > 0);  // non-leaf nodes should have at least one child
                // if (!node.childCnt) {
                //     GeneralLogger::error("Failed to lookup color while quantizing: dead end");
                //     return 0;
                // }
                // // single child, just go to it
                // else if (node.childCnt == 1) {
                //     for (uint32_t child = 0; child < 8; ++child) {
                //         if (node.u.childIdxs[child]) {
                //             childIdx = node.u.childIdxs[child];
                //             break;
                //         }
                //     }
                // }
                // // more than 1 children, find the closest one
                // else {
                //     uint32_t closestIndex = 8;
                //     uint32_t minDist      = 8;  // 8 > 7 for sure
                //     for (uint32_t child = 0; child < 8; ++child) {
                //         if (child != index && node.u.childIdxs[child]) {
                //             const auto dist = _getDistance(index, child);
                //             if (dist < minDist) {
                //                 minDist      = dist;
                //                 closestIndex = child;
                //             }
                //         }
                //     }
                //     childIdx = node.u.childIdxs[closestIndex];
                // }

                // or just return 0, let the caller handle it :)
                // actually only a few pixels could reach here, less than 10%, likely 1%,
                // and most of them only happen when applying error-diffusion dithering
                // _BEEP_CNT++;
                return 0;
            }
            nodeIdx = childIdx;
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

    void
    addColor(const PixelBGRA& color) noexcept {
        if (isFinished) return;
        assert(!m_gray || (color.r == color.g && color.g == color.b));  // grayscale colors only
        isValid      = true;
        auto indexes = _octIndex(color);
        auto nodeIdx = 0;
        for (int i = 0; i < 8; ++i) {
            // get index
            const auto index = indexes[i];
            // if no child node, allocate a new one
            auto childIdx = m_nodes[nodeIdx].u.childIdxs[index];
            if (!childIdx) {
                m_nodes[nodeIdx].u.childIdxs[index] = _allocNode(i + 1);
                // ATTENTION: _allocNode() will push a new node to m_nodes,
                //            which could invalidate the reference to node
                m_nodes[nodeIdx].childCnt += 1;
                m_nodes[m_nodes[nodeIdx].u.childIdxs[index]].parentIdx = nodeIdx;
            }
            ++(m_nodes[nodeIdx].count);
            assert(m_nodes[nodeIdx].childCnt > 0);  // non-leaf nodes should have at least one child
            // move to next level
            nodeIdx = m_nodes[nodeIdx].u.childIdxs[index];
        }
        auto& node = m_nodes[nodeIdx];
        node.u.sum[0] += color.r;
        node.u.sum[1] += color.g;
        node.u.sum[2] += color.b;
        node.count += 1;
        node.isLeaf = true;  // mark as leaf
    }

    std::vector<PixelBGRA>
    buildPalette() noexcept {
        if (isFinished) return {};
        std::vector<uint32_t> finalNodeIdxs;
        if (m_levels[8].size() > m_numColors) {
            _reduce(finalNodeIdxs);
        } else {
            finalNodeIdxs.resize(m_levels[8].size());
            std::memcpy(finalNodeIdxs.data(),
                        m_levels[8].data(),
                        m_levels[8].size() * sizeof(uint32_t));
        }
        std::vector<PixelBGRA> colors(finalNodeIdxs.size());
        // first: grayscale, second: index in finalNodeIdxs
        std::vector<std::pair<uint32_t, uint32_t>> grays(finalNodeIdxs.size());
        m_palette.reserve(finalNodeIdxs.size());
        uint32_t idx = 0;
        for (const auto nodeIdx : finalNodeIdxs) {
            const auto& node = m_nodes[nodeIdx];
            // calculate the average color
            const uint8_t r = TOU8(node.u.sum[0] / node.count);
            const uint8_t g = TOU8(node.u.sum[1] / node.count);
            const uint8_t b = TOU8(node.u.sum[2] / node.count);

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
            m_colorMap[finalNodeIdxs[gray.second]] = idx++;
            m_palette.push_back(colors[gray.second]);
        }
        m_palette.resize(m_numColors);
        for (uint32_t i = grays.size(); i < m_numColors; ++i) {
            m_palette[i] = makeBGRA(0, 0, 0);  // fill with black
        }
        isFinished = true;
        return m_palette;
    }

    std::vector<PixelBGRA>
    getPalette() const noexcept {
        if (!isFinished) return {};
        return m_palette;
    }

    // get index in m_palette
    uint32_t
    getIndice(const PixelBGRA& color) const noexcept {
        const auto index = _getIndex(color);
        return index ? m_colorMap.at(index) : findClosestColor(color);
    }

    // uint32_t
    // findClosestColor(const PixelBGRA& color) const noexcept {
    //     if (!isFinished) return 0;
    //     auto nodeIdx = _getIndex(color);
    //     // case not found
    //     if (!nodeIdx) return 0;
    //     // case called with a color not in the palette
    //     if (color != m_palette[m_colorMap.at(nodeIdx)]) {
    //         return nodeIdx;
    //     }
    //     // find the closest ancestor with more than one child
    //     auto parentIdx = m_nodes[nodeIdx].parentIdx;
    //     while (parentIdx && m_nodes[parentIdx].childCnt <= 1) {
    //         nodeIdx   = parentIdx;
    //         parentIdx = m_nodes[nodeIdx].parentIdx;
    //     }
    //     // case such ancestor does not exist
    //     if (!parentIdx) return 0;
    //     const auto indexes  = _octIndex(m_palette[nodeIdx]);
    //     const auto& parent  = m_nodes[parentIdx];
    //     const auto index    = indexes[parent.level];
    //     uint32_t closestIdx = 8;
    //     uint32_t minDist    = 8;  // 8 > 7 for sure
    //     for (uint32_t child = 0; child < 8; ++child) {
    //         if (parent.u.childIdxs[child] && parent.u.childIdxs[child] != nodeIdx) {
    //             const auto dist = _getDistance(index, child);
    //             if (dist < minDist) {
    //                 minDist    = dist;
    //                 closestIdx = parent.u.childIdxs[child];
    //             }
    //         }
    //     }
    //     nodeIdx = closestIdx;
    //     while (true) {
    //         auto& node = m_nodes[nodeIdx];
    //         if (node.isLeaf) {
    //             break;
    //         }
    //         assert(node.level < 8);  // dead end
    //         if (node.level == 8) {
    //             GeneralLogger::error("Failed to find closest color while quantizing: dead end at last");
    //             return 0;
    //         }
    //         assert(node.childCnt > 0);  // non-leaf nodes should have at least one child
    //         if (!node.childCnt) {
    //             GeneralLogger::error("Failed to find closest color while quantizing: dead end");
    //             return 0;
    //         }
    //         // it doesn't matter now which child is the closest, just go with the first one
    //         for (const auto& childIdx : node.u.childIdxs) {
    //             if (childIdx) {
    //                 nodeIdx = childIdx;
    //                 break;
    //             }
    //         }
    //     }
    //     return m_colorMap.at(nodeIdx);
    // }

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

    bool
    valid() const noexcept {
        return isValid;
    }

  private:
    uint32_t m_numColors;
    bool isFinished = false;
    bool isValid    = true;  // will be true if any color is added
    bool m_gray     = false;

    std::vector<Node> m_nodes;
    std::array<std::vector<uint32_t>, 9> m_levels;
    std::vector<PixelBGRA> m_palette;
    std::unordered_map<uint32_t, uint8_t> m_colorMap;  // idx in m_nodes to idx in m_palette
};

static std::pair<uint32_t, uint32_t>
downsampleStep(uint32_t width, uint32_t height, uint32_t maxPixels) noexcept {
    if (width * height <= DOWNSAMPLE_THRESHOLD) {
        return {1, 1};  // no downsampling needed
    }
    uint32_t step  = static_cast<uint32_t>(std::ceil(std::sqrt(static_cast<double>(width) * height / maxPixels)));
    uint32_t stepW = step, stepH = step;
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

    std::fill(indices.begin(), indices.end(), 0);
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
            pltIndex                  = quantizer.getIndice(orgColor);

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

// ordered dithering with multiple (grayscale) colors
static void
orderedDithering(const std::span<PixelBGRA>& pixels,
                 const std::span<PixelBGRA>& sortedPalette,
                 const std::span<PixelBGRA>& sortedPaletteGray,
                 std::vector<uint8_t>& indices,
                 const uint32_t width,
                 const uint32_t height,
                 const uint32_t numColors,
                 const bool hasTransparency,
                 const uint8_t transparentThreshold) noexcept {
    static constexpr int32_t BayerMat[4][4] = {{0, 8, 2, 10},
                                               {12, 4, 14, 6},
                                               {3, 11, 1, 9},
                                               {15, 7, 13, 5}};

    for (uint32_t y = 0, idx = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++, idx++) {
            const uint8_t index = indices[idx];
            // skip transparent pixels
            if (hasTransparency && pixels[idx].a <= transparentThreshold) {
                continue;
            }
            // exactly the same color
            if (pixels[idx] == sortedPalette[index]) continue;
            // compare in GrayScale
            const uint32_t grayPalette = sortedPaletteGray[index].r;
            // palette brighter
            if (const uint32_t grayOrig = pixels[idx].r; grayPalette > grayOrig) {
                // already the darkest
                if (index == numColors - 1) continue;
                const int32_t grayNext = sortedPaletteGray[index + 1].r;
                const int32_t factor   = BayerMat[y % 4][x % 4];
                if ((grayOrig - grayNext) * 16 < (grayPalette - grayNext) * factor) {
                    indices[idx] = index + 1;
                } else {
                    indices[idx] = index;
                }
            }
            // darker
            else {
                // already the brightest
                if (index == 0) continue;
                const int32_t grayPrev = sortedPaletteGray[index - 1].r;
                const int32_t factor   = BayerMat[y % 4][x % 4];
                if ((grayOrig - grayPalette) * 16 >= (grayPrev - grayPalette) * factor) {
                    indices[idx] = index - 1;
                } else {
                    indices[idx] = index;
                }
            }
        }
    }
}

// #include <chrono>

GIFImage::QuantizerResult
GIFImage::quantize(const std::span<PixelBGRA>& pixels,
                   uint32_t width,
                   uint32_t height,
                   uint32_t numColors,
                   GIFImage::DitherMode ditherMode,
                   bool grayScale,
                   bool transparency,
                   uint8_t transparentThreshold,
                   bool downsample) noexcept {
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

    // GeneralLogger::info("Starting quantization");
    // auto start = std::chrono::high_resolution_clock::now();
    OctreeQuantizer quantizer(numColors, grayScale);
    QuantizerResult result{
        .hasTransparency   = transparency,
        .width             = width,
        .height            = height,
        .transparencyIndex = transparency ? numColors : 0,
    };
    // auto now = std::chrono::high_resolution_clock::now();
    // printf("%ld ms\n",
    //        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
    // start = now;

    // add pixels to quantizer
    auto [stepW, stepH] = downsampleStep(width, height, DOWNSAMPLE_THRESHOLD);
    // GeneralLogger::info("Downsampling steps: " + std::to_string(stepW) + "x" + std::to_string(stepH));
    // GeneralLogger::info("Adding pixels to quantizer");
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
                    const auto c = makeBGRA(TOU8(b / count), TOU8(g / count), TOU8(r / count));
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
    // now = std::chrono::high_resolution_clock::now();
    // printf("%ld ms\n",
    //        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
    // start = now;

    // If no pixels were added
    if (!quantizer.valid()) {
        GeneralLogger::warn("All pixels are considered as transparent, skipping quantization.");
        return QuantizerResult{
            .isValid      = false,
            .errorMessage = "All pixels are transparent",
        };
    }

    // GeneralLogger::info("Building palette");
    result.palette = quantizer.buildPalette();
    // now            = std::chrono::high_resolution_clock::now();
    // printf("%ld ms\n",
    //        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
    // start = now;
    // GeneralLogger::info("Querying color indices");
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
            result.indices.push_back(quantizer.getIndice(grayScale ? toGray(pixel) : pixel));
        }
    }
    // now = std::chrono::high_resolution_clock::now();
    // printf("%ld ms\n",
    //        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
    // start = now;

    // apply dithering if needed
    // GeneralLogger::info("Applying dithering");
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
        // already sorted in quantizer
        auto& sortedPalette = result.palette;
        // convert palette to grayscale
        auto sortedPaletteGray = std::vector<PixelBGRA>(sortedPalette.size());
        for (size_t i = 0; i < sortedPalette.size(); ++i) {
            sortedPaletteGray[i] = toGray(sortedPalette[i]);
        }
        orderedDithering(pixels, sortedPalette, sortedPaletteGray, result.indices, width, height, numColors, transparency, transparentThreshold);
    }
    // now = std::chrono::high_resolution_clock::now();
    // printf("%ld ms\n",
    //        std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
    // start = now;

    // GeneralLogger::info("Quantization completed successfully");
    // for (const auto& color : result.palette) {
    //     printf("#%02x%02x%02x\n", color.r, color.g, color.b);
    // }
    // printf("Beeped %d times, %.2f%%\n", _BEEP_CNT, static_cast<double>(_BEEP_CNT) / pixels.size() * 100.0);
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