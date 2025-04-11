#ifndef GIF_MIRAGE_IMAGE_SEQUENCE_H
#define GIF_MIRAGE_IMAGE_SEQUENCE_H

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "def.h"

namespace GIFImage {

// Forward declaration
class ImageSequence;

using ImageSequenceRef = std::unique_ptr<ImageSequence>;

class ImageSequence {
  public:
    static bool
    initDecoder(const char*) noexcept;

    static constexpr u32 DEFAULT_DELAY = 40;  // Default delay in milliseconds

    static ImageSequenceRef
    read(const std::string& filename) noexcept;

    static bool
    drawText(std::span<PixelBGRA>& buffer,
             u32 width,
             u32 height,
             const std::string& text,
             const PixelBGRA& textBackgroundColor = {0x40, 0x40, 0x40, 0xA0},
             const PixelBGRA& textForegroundColor = {0xDD, 0xDD, 0xDD, 0xA0},
             float textHeightRatio                = 0.04f,
             float textPadding                    = 0.05f,
             const std::string& fontFamily        = "Arial") noexcept;

    virtual ~ImageSequence() = default;

    virtual inline std::vector<u32>&
    getDelays() noexcept = 0;

    /**
     * @brief Get the frame buffer of the specified index.
     *        If width and height are not specified, the original size will be used.
     *        Otherwise, the returned buffer will be resized with "cover" mode.
     *
     * @param[in] index The index of the frame to get.
     * @param[in] width The width of the frame buffer. 0 means original width.
     * @param[in] height The height of the frame buffer. 0 means original height.
     * @param[in] ensureAccurate If true, the frame buffer will not be resized.
     *
     * @return The frame buffer of the specified index in BGRA8888 format.
     */
    virtual std::vector<PixelBGRA>
    getFrameBuffer(u32 index, u32 width, u32 height, bool ensureAccurate) noexcept = 0;

    [[nodiscard]] virtual u32
    getFrameCount() const noexcept = 0;

    [[nodiscard]] virtual u32
    getWidth() const noexcept = 0;

    [[nodiscard]] virtual u32
    getHeight() const noexcept = 0;
};

}  // namespace GIFImage

#endif  // GIF_MIRAGE_IMAGE_SEQUENCE_H