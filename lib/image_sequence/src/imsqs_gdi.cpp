// There's actually no difference between this streaming implementation
// and the normal one in
#include "imsq.h"
// when
#ifdef IMSQ_USE_GDIPLUS
// since GDI + will read the whole image into memory on first access anyway.

#include "imsq_exception.h"
#include "imsq_stream.h"

using namespace GIFImage;
using std::string;

class ImageSequenceStreamGdiplusImpl : public ImageSequenceStream {
  public:
    explicit ImageSequenceStreamGdiplusImpl(const string&);

    ~ImageSequenceStreamGdiplusImpl() override = default;

    Frame::Ref
    getNextFrame() noexcept override;

    [[nodiscard]] bool
    isEndOfStream() const noexcept override;

  private:
    ImageSequence::Ref m_imsq = nullptr;
    uint32_t m_currFrame      = 0;
};

bool
ImageSequenceStream::initDecoder(const char*) noexcept {
    return ImageSequence::initDecoder("");
}

ImageSequenceStream::Ref
ImageSequenceStream::read(const string& filename) noexcept {
    try {
        return std::make_unique<ImageSequenceStreamGdiplusImpl>(filename);
    } catch (const ImageParseException& e) {
        return nullptr;
    }
}

ImageSequenceStreamGdiplusImpl::ImageSequenceStreamGdiplusImpl(const string& filename) {
    m_imsq = ImageSequence::read(filename);
    if (!m_imsq) {
        throw ImageParseException("Failed to read image sequence: " + filename);
    }
}

bool
ImageSequenceStreamGdiplusImpl::isEndOfStream() const noexcept {
    return m_currFrame >= m_imsq->getFrameCount();
}

Frame::Ref
ImageSequenceStreamGdiplusImpl::getNextFrame() noexcept {
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