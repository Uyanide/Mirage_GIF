#include <cstdint>
#include <vector>

#include "def.h"
#include "imsq.h"
#include "imsq_exception.h"
#include "log.h"

class ImageSequenceNativeImpl : public GIFImage::ImageSequence {
  public:
    ImageSequenceNativeImpl(const std::vector<std::vector<PixelBGRA>>& frames,
                            const std::span<const uint32_t>& delays,
                            uint32_t width,
                            uint32_t height) noexcept
        : m_delays(delays.begin(), delays.end()),
          m_width(width),
          m_height(height) {
        m_frames.reserve(frames.size());
        for (const auto& frame : frames) {
            m_frames.emplace_back(frame.begin(), frame.end());
        }
    }

    ~ImageSequenceNativeImpl() noexcept override = default;

    [[nodiscard]] const std::vector<uint32_t>&
    getDelays() noexcept override {
        return m_delays;
    }

    [[nodiscard]] std::vector<PixelBGRA>
    getFrameBuffer(uint32_t index, uint32_t width, uint32_t height) noexcept override {
        if (width != m_width || height != m_height) {
            return {};
        }
        if (index >= m_frames.size()) {
            index %= m_frames.size();
        }
        return m_frames[index];
    }

    [[nodiscard]] uint32_t
    getWidth() const noexcept override {
        return m_width;
    }

    [[nodiscard]] uint32_t
    getHeight() const noexcept override {
        return m_height;
    }

    [[nodiscard]] uint32_t
    getFrameCount() const noexcept override {
        return static_cast<uint32_t>(m_frames.size());
    }

  private:
    std::vector<std::vector<PixelBGRA>> m_frames;
    std::vector<uint32_t> m_delays;
    uint32_t m_width;
    uint32_t m_height;
};

GIFImage::ImageSequence::Ref
GIFImage::ImageSequence::load(const std::vector<std::vector<PixelBGRA>>& frames,
                              const std::span<const uint32_t>& delays,
                              uint32_t width,
                              uint32_t height) noexcept {
    if (frames.empty() || delays.empty()) {
        return nullptr;
    }
    if (frames.size() != delays.size()) {
        return nullptr;
    }
    if (width == 0 || height == 0) {
        return nullptr;
    }
    for (const auto& frame : frames) {
        if (frame.size() != width * height) {
            return nullptr;
        }
    }

    return std::make_unique<ImageSequenceNativeImpl>(frames, delays, width, height);
}

bool
GIFImage::ImageSequence::drawMark(std::vector<PixelBGRA>& buffer,
                                  const uint32_t width,
                                  const uint32_t height,
                                  const std::vector<PixelBGRA>& markBuffer,
                                  const uint32_t markWidth,
                                  const uint32_t markHeight,
                                  const uint32_t x,
                                  const uint32_t y) noexcept {
    try {
        if (width == 0 || height == 0 || markWidth == 0 || markHeight == 0) {
            throw ImageParseException("Invalid dimensions for drawing mark.");
        }
        if (buffer.size() != width * height) {
            throw ImageParseException("Buffer size does not match dimensions: " + std::to_string(buffer.size()) +
                                      " != " + std::to_string(width * height));
        }
        if (markBuffer.size() != markWidth * markHeight) {
            throw ImageParseException("Mark buffer size does not match dimensions: " +
                                      std::to_string(markBuffer.size()) + " != " +
                                      std::to_string(markWidth * markHeight));
        }
        if (x >= width || y >= height) {
            throw ImageParseException("Invalid x or y coordinates for drawing mark.");
        }

        for (uint32_t i = y; i < y + markHeight && i < height; ++i) {
            for (uint32_t j = x; j < x + markWidth && j < width; ++j) {
                auto& buff         = buffer[i * width + j];
                const auto& mark   = markBuffer[(i - y) * markWidth + (j - x)];
                const double alpha = mark.a / 255.0;

                buff.r = TOU8(mark.r * alpha + buff.r * (1 - alpha));
                buff.g = TOU8(mark.g * alpha + buff.g * (1 - alpha));
                buff.b = TOU8(mark.b * alpha + buff.b * (1 - alpha));
                buff.a = TOU8(mark.a + buff.a * (1 - alpha));
            }
        }
        return true;
    } catch (const std::exception& e) {
        GeneralLogger::error("Error drawing mark: " + std::string(e.what()));
    } catch (...) {
        GeneralLogger::error("Unknown error drawing mark.");
    }
    return false;
}

#ifdef IMSQ_USE_NATIVE

std::vector<PixelBGRA>
GIFImage::ImageSequence::resizeCover(const std::vector<PixelBGRA>& buffer,
                                     uint32_t origWidth,
                                     uint32_t origHeight,
                                     uint32_t targetWidth,
                                     uint32_t targetHeight) noexcept {
    if (origWidth == 0 || origHeight == 0 || targetWidth == 0 || targetHeight == 0) {
        return {};
    }
    if (buffer.size() != origWidth * origHeight) {
        return {};
    }
    if (targetWidth == origWidth && targetHeight == origHeight) {
        return buffer;
    }

    double origAspect   = static_cast<double>(origWidth) / origHeight;
    double targetAspect = static_cast<double>(targetWidth) / targetHeight;

    uint32_t resizedWidth, resizedHeight;
    uint32_t offsetX = 0, offsetY = 0;

    if (origAspect > targetAspect) {
        resizedHeight = targetHeight;
        resizedWidth  = static_cast<uint32_t>(targetHeight * origAspect);
        offsetX       = (resizedWidth - targetWidth) / 2;
    } else {
        resizedWidth  = targetWidth;
        resizedHeight = static_cast<uint32_t>(targetWidth / origAspect);
        offsetY       = (resizedHeight - targetHeight) / 2;
    }

    std::vector<PixelBGRA> resizedBuffer(resizedWidth * resizedHeight);

    for (uint32_t y = 0; y < resizedHeight; ++y) {
        for (uint32_t x = 0; x < resizedWidth; ++x) {
            double srcX = x * static_cast<double>(origWidth - 1) / (resizedWidth - 1);
            double srcY = y * static_cast<double>(origHeight - 1) / (resizedHeight - 1);

            if (resizedWidth == 1) srcX = 0;
            if (resizedHeight == 1) srcY = 0;

            uint32_t x0 = static_cast<uint32_t>(srcX);
            uint32_t y0 = static_cast<uint32_t>(srcY);
            uint32_t x1 = std::min(x0 + 1, origWidth - 1);
            uint32_t y1 = std::min(y0 + 1, origHeight - 1);

            double wx = srcX - x0;
            double wy = srcY - y0;

            const PixelBGRA& p00 = buffer[y0 * origWidth + x0];
            const PixelBGRA& p01 = buffer[y0 * origWidth + x1];
            const PixelBGRA& p10 = buffer[y1 * origWidth + x0];
            const PixelBGRA& p11 = buffer[y1 * origWidth + x1];

            double r = (1 - wx) * (1 - wy) * p00.r + wx * (1 - wy) * p01.r +
                       (1 - wx) * wy * p10.r + wx * wy * p11.r;
            double g = (1 - wx) * (1 - wy) * p00.g + wx * (1 - wy) * p01.g +
                       (1 - wx) * wy * p10.g + wx * wy * p11.g;
            double b = (1 - wx) * (1 - wy) * p00.b + wx * (1 - wy) * p01.b +
                       (1 - wx) * wy * p10.b + wx * wy * p11.b;
            double a = (1 - wx) * (1 - wy) * p00.a + wx * (1 - wy) * p01.a +
                       (1 - wx) * wy * p10.a + wx * wy * p11.a;

            PixelBGRA& result = resizedBuffer[y * resizedWidth + x];
            result.r          = TOU8(r);
            result.g          = TOU8(g);
            result.b          = TOU8(b);
            result.a          = TOU8(a);
        }
    }

    std::vector<PixelBGRA> targetBuffer(targetWidth * targetHeight);

    for (uint32_t y = 0; y < targetHeight; ++y) {
        for (uint32_t x = 0; x < targetWidth; ++x) {
            uint32_t srcX = x + offsetX;
            uint32_t srcY = y + offsetY;

            if (srcX >= resizedWidth) srcX = resizedWidth - 1;
            if (srcY >= resizedHeight) srcY = resizedHeight - 1;

            targetBuffer[y * targetWidth + x] = resizedBuffer[srcY * resizedWidth + srcX];
        }
    }

    return targetBuffer;
}

bool
GIFImage::ImageSequence::initDecoder(const char*) noexcept {
    return true;
}

GIFImage::ImageSequence::Ref
GIFImage::ImageSequence::read(const std::string&) noexcept {
    GeneralLogger::error("Failed to decode image: No codec available");
    return nullptr;
}

std::vector<PixelBGRA>
GIFImage::ImageSequence::parseBase64(const std::string&) noexcept {
    GeneralLogger::error("Failed to decode base64 image: No codec available");
    return {};
}

bool
GIFImage::ImageSequence::drawText(std::vector<PixelBGRA>&,
                                  uint32_t,
                                  uint32_t,
                                  const std::string&,
                                  const PixelBGRA&,
                                  const PixelBGRA&,
                                  double,
                                  double,
                                  uint32_t,
                                  uint32_t,
                                  const std::string&) noexcept {
    return false;
}

#endif  // IMSQ_USE_NATIVE