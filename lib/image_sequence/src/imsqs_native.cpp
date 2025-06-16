#include "imsq_stream.h"

using namespace GIFImage;

class ImageSequenceStreamNativeImpl : public ImageSequenceStream {
  public:
    explicit ImageSequenceStreamNativeImpl(const std::vector<std::vector<PixelBGRA>>& frames,
                                           const std::span<const uint32_t>& delays,
                                           const std::span<const uint32_t> widths,
                                           const std::span<const uint32_t> heights) noexcept {
        m_frames.resize(frames.size());
        for (size_t i = 0; i < frames.size(); ++i) {
            m_frames[i].width  = widths[i];
            m_frames[i].height = heights[i];
            m_frames[i].delay  = delays[i];
            m_frames[i].buffer.resize(m_frames[i].width * m_frames[i].height);
            std::copy(frames[i].begin(), frames[i].end(), m_frames[i].buffer.begin());
        }
    }

    ~ImageSequenceStreamNativeImpl() noexcept override = default;

    Frame::Ref
    getNextFrame() noexcept override {
        if (m_currFrame >= m_frames.size()) {
            return nullptr;
        }
        Frame::Ref frame = std::make_unique<Frame>();
        frame->width     = m_frames[m_currFrame].width;
        frame->height    = m_frames[m_currFrame].height;
        frame->delay     = m_frames[m_currFrame].delay;
        frame->buffer    = std::move(m_frames[m_currFrame].buffer);
        ++m_currFrame;
        return frame;
    }

    [[nodiscard]] bool
    isEndOfStream() const noexcept override {
        return m_currFrame >= m_frames.size();
    }

  private:
    std::vector<Frame> m_frames;
    uint32_t m_currFrame = 0;
};

ImageSequenceStream::Ref
ImageSequenceStream::load(const std::vector<std::vector<PixelBGRA>>& frames,
                          const std::span<const uint32_t>& delays,
                          const std::span<const uint32_t> widths,
                          const std::span<const uint32_t> heights) noexcept {
    if (frames.empty()) {
        return nullptr;
    }
    if (frames.size() != delays.size() ||
        frames.size() != widths.size() ||
        frames.size() != heights.size()) {
        return nullptr;
    }
    for (const auto& frame : frames) {
        if (frame.size() != widths[0] * heights[0]) {
            return nullptr;
        }
    }
    for (const auto& v : widths) {
        if (v == 0) {
            return nullptr;
        }
    }
    for (const auto& v : heights) {
        if (v == 0) {
            return nullptr;
        }
    }
    return std::make_unique<ImageSequenceStreamNativeImpl>(frames, delays, widths, heights);
}

#ifdef IMSQ_USE_NATIVE

#include "log.h"

bool
ImageSequenceStream::initDecoder(const char*) noexcept {
    return true;
}

ImageSequenceStream::Ref
ImageSequenceStream::read(const std::string&) noexcept {
    GeneralLogger::error("Failed to decode image: No codec available");
    return nullptr;
}

#endif  // IMSQ_USE_NATIVE