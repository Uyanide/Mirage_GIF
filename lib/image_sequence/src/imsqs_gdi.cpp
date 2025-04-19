// There's actually no difference between this streaming implementation
// and the normal one in
#include "imsq.h"
// when
#ifdef IMSQ_USE_GDIPLUS
// since GDI + will read the whole image into memory on first access anyway.

#include "imsq_stream.h"

using namespace GIFImage;
using std::string;

class ImageSequenceStreamImpl : public ImageSequenceStream {
  public:
    explicit ImageSequenceStreamImpl(const string&);

    ~ImageSequenceStreamImpl() override = default;

    Frame::Ref
    getNextFrame() noexcept override;

    [[nodiscard]] bool
    isEndOfStream() const noexcept override;

  private:
    ImageSequence::Ref m_imsq = nullptr;
    uint32_t m_currFrame      = 0;
};

class ImageParseException final : public std::exception {
  public:
    explicit ImageParseException(const string&& message)
        : m_msg(message) {}

    [[nodiscard]] const char*
    what() const noexcept override {
        return m_msg.c_str();
    }

  private:
    string m_msg;
};

bool
ImageSequenceStream::initDecoder(const char*) noexcept {
    return ImageSequence::initDecoder("");
}

ImageSequenceStream::Ref
ImageSequenceStream::read(const string& filename) noexcept {
    try {
        return std::make_unique<ImageSequenceStreamImpl>(filename);
    } catch (const ImageParseException& e) {
        return nullptr;
    }
}

ImageSequenceStreamImpl::ImageSequenceStreamImpl(const string& filename) {
    m_imsq = ImageSequence::read(filename);
    if (!m_imsq) {
        throw ImageParseException("Failed to read image sequence: " + filename);
    }
}

bool
ImageSequenceStreamImpl::isEndOfStream() const noexcept {
    return m_currFrame >= m_imsq->getFrameCount();
}

Frame::Ref
ImageSequenceStreamImpl::getNextFrame() noexcept {
    while (true) {
        if (isEndOfStream()) {
            return nullptr;
        }
        auto frame = m_imsq->getFrameBuffer(m_currFrame++, 0, 0);
        if (frame.empty()) {
            continue;
        }

        auto frameRef    = std::make_unique<Frame>();
        frameRef->buffer = std::move(frame);
        frameRef->width  = m_imsq->getWidth();
        frameRef->height = m_imsq->getHeight();
        frameRef->delay  = m_imsq->getDelays()[m_currFrame - 1];

        return frameRef;
    }
}

#endif  // IMSQ_USE_GDIPLUS