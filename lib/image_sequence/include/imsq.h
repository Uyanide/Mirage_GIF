#ifndef GIF_MIRAGE_IMAGE_SEQUENCE_H
#define GIF_MIRAGE_IMAGE_SEQUENCE_H

#include <memory>
#include <string>
#include <vector>

#include "def.h"

namespace GIFImage {

// Forward declaration
class ImageSequence;

class ImageSequence {
  public:
    using Ref = std::unique_ptr<ImageSequence>;

    static bool
    initDecoder(const char*) noexcept;

    static constexpr uint32_t DEFAULT_DELAY = 40;  // Default delay in milliseconds

    static Ref
    read(const std::string& filename) noexcept;

    static Ref
    load(const std::span<const std::span<const PixelBGRA>>& frames,
         const std::span<const uint32_t>& delays,
         uint32_t width,
         uint32_t height) noexcept;

    static std::vector<PixelBGRA>
    parseBase64(const std::string& base64) noexcept;

    static std::vector<PixelBGRA>
    resizeCover(const std::vector<PixelBGRA>& buffer,
                uint32_t origWidth,
                uint32_t origHeight,
                uint32_t targetWidth,
                uint32_t targetHeight) noexcept;

    static bool
    drawText(std::vector<PixelBGRA>& buffer,
             uint32_t width,
             uint32_t height,
             const std::string& text,
             const PixelBGRA& textBackgroundColor = {0x30, 0x30, 0x30, 0xA0},
             const PixelBGRA& textForegroundColor = {0xC0, 0xC0, 0xC0, 0xff},
             double textHeightRatio               = 0.04,
             double textPadding                   = 0.05,
             uint32_t x                           = 0,
             uint32_t y                           = 0,
             const std::string& fontFamily        = "Arial") noexcept;

    static bool
    drawMark(std::vector<PixelBGRA>& buffer,
             uint32_t width,
             uint32_t height,
             const std::vector<PixelBGRA>& markBuffer,
             uint32_t markWidth,
             uint32_t markHeight,
             uint32_t x = 0,
             uint32_t y = 0) noexcept;

    virtual ~ImageSequence() = default;

    [[nodiscard]] virtual const std::vector<uint32_t>&
    getDelays() noexcept = 0;

    /**
     * @brief Get the frame buffer of the specified index.
     *        If width and height are not specified, the original size will be used.
     *        Otherwise, the returned buffer will be resized with "cover" mode.
     *
     * @param[in] index The index of the frame to get.
     * @param[in] width The width of the frame buffer. 0 means original width.
     * @param[in] height The height of the frame buffer. 0 means original height.
     *
     * @return The frame buffer of the specified index in BGRA8888 format.
     */
    [[nodiscard]] virtual std::vector<PixelBGRA>
    getFrameBuffer(uint32_t index,
                   uint32_t width,
                   uint32_t height) noexcept = 0;

    [[nodiscard]] virtual uint32_t
    getFrameCount() const noexcept = 0;

    [[nodiscard]] virtual uint32_t
    getWidth() const noexcept = 0;

    [[nodiscard]] virtual uint32_t
    getHeight() const noexcept = 0;
};

}  // namespace GIFImage

#endif  // GIF_MIRAGE_IMAGE_SEQUENCE_H