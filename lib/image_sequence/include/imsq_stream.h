#ifndef GIF_MIRAGE_IMAGE_SEQUENCE_STREAM_H
#define GIF_MIRAGE_IMAGE_SEQUENCE_STREAM_H

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "def.h"


namespace GIFImage {

class ImageSequenceStream;

static constexpr uint32_t DEFAULT_DELAY = 40;

struct Frame {
    using Ref = std::unique_ptr<Frame>;

    std::vector<PixelBGRA> buffer;
    uint32_t width  = 0;
    uint32_t height = 0;
    uint32_t delay  = DEFAULT_DELAY;  // Delay in milliseconds
};

class ImageSequenceStream {
  public:
    using Ref = std::unique_ptr<ImageSequenceStream>;

    static bool
    initDecoder(const char*) noexcept;

    static Ref
    read(const std::string&) noexcept;

    static Ref
    load(const std::span<const std::span<const PixelBGRA>>& frames,
         const std::span<const uint32_t>& delays,
         const std::span<const uint32_t> widths,
         const std::span<const uint32_t> heights) noexcept;

    virtual ~ImageSequenceStream() = default;

    virtual Frame::Ref
    getNextFrame() noexcept = 0;

    [[nodiscard]] virtual bool
    isEndOfStream() const noexcept = 0;
};

}  // namespace GIFImage

#endif  // GIF_MIRAGE_IMAGE_SEQUENCE_STREAM_H