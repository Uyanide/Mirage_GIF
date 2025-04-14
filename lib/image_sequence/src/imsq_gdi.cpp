#ifdef IMSQ_USE_GDIPLUS

#include <windows.h>
//
#include <gdiplus.h>
#include <webp/decode.h>
#include <webp/demux.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <unordered_map>

#include "gdi_initializer.h"
#include "imsq.h"
#include "log.h"
#include "path.h"

using std::string, std::vector, std::wstring;

static constexpr const char* FALLBACK_FONT = "Arial";

class ImageParseException final : public std::exception {
  public:
    explicit ImageParseException(const std::string&& message) : m_msg(message) {}

    [[nodiscard]] const char*
    what() const noexcept override {
        return m_msg.c_str();
    }

  private:
    std::string m_msg;
};

class ImageSequenceImpl : public GIFImage::ImageSequence {
  public:
    explicit ImageSequenceImpl(const std::string& filename);

    ~ImageSequenceImpl() override;

    [[nodiscard]] const std::vector<uint32_t>&
    getDelays() noexcept override {
        return m_delays;
    }

    [[nodiscard]] uint32_t
    getFrameCount() const noexcept override {
        return static_cast<uint32_t>(m_delays.size());
    }

    [[nodiscard]] uint32_t
    getWidth() const noexcept override {
        return m_width;
    }

    [[nodiscard]] uint32_t
    getHeight() const noexcept override {
        return m_height;
    }

    std::vector<PixelBGRA>
    getFrameBuffer(uint32_t index, uint32_t width, uint32_t height) noexcept override;

  private:
    Gdiplus::Image* m_image                = nullptr;
    Gdiplus::PropertyItem* m_delayPropItem = nullptr;
    std::vector<uint32_t> m_delays;
    uint32_t m_width  = 0;
    uint32_t m_height = 0;
    std::mutex m_imageMutex;
};

class ImageSequenceWebpImpl : public GIFImage::ImageSequence {
  public:
    explicit ImageSequenceWebpImpl(const std::string& filename);
    ~ImageSequenceWebpImpl() override;

    [[nodiscard]] const std::vector<uint32_t>&
    getDelays() noexcept override {
        return m_delays;
    }

    [[nodiscard]] uint32_t
    getFrameCount() const noexcept override {
        return m_frameCount;
    }

    [[nodiscard]] uint32_t
    getWidth() const noexcept override {
        return m_width;
    }

    [[nodiscard]] uint32_t
    getHeight() const noexcept override {
        return m_height;
    }

    std::vector<PixelBGRA>
    getFrameBuffer(uint32_t index, uint32_t width, uint32_t height) noexcept override;

  private:
    WebPData m_webpData{};
    WebPDemuxer* m_demux = nullptr;
    WebPDecoderConfig m_config{};
    std::vector<uint32_t> m_delays;
    uint32_t m_frameCount = 0;
    uint32_t m_width      = 0;
    uint32_t m_height     = 0;
    std::mutex m_demuxMutex;
};

std::unique_ptr<GIFImage::ImageSequence>
GIFImage::ImageSequence::read(const std::string& filename) noexcept {
    try {
        std::string ext = filename.substr(filename.find_last_of('.') + 1);
        for (auto& c : ext) c = static_cast<char>(tolower(c));

        if (ext == "webp") {
            return std::make_unique<ImageSequenceWebpImpl>(filename);
        } else {
            return std::make_unique<ImageSequenceImpl>(filename);
        }
    } catch (const std::exception& e) {
        GeneralLogger::error("Error reading image sequence: " + string(e.what()));
        return nullptr;
    }
}

// GIFImage::ImageSequence::ImageSequence() = default;

static auto
checkFileExists(const std::string& filename) {
    auto localized = localizePath(filename);
    if (const std::filesystem::path filePath(localized); std::filesystem::exists(filePath)) {
        return localized;
    } else {
        return decltype(localized)();
    }
}

static bool
ReadFileToWebPData(const std::string& filename, WebPData* webp_data) {
    const auto localized = checkFileExists(filename);
    if (localized.empty()) {
        return false;
    }
    const std::filesystem::path filePath(localized);
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }

    const size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    auto* data = new uint8_t[fileSize];
    if (!file.read(reinterpret_cast<char*>(data), fileSize)) {
        delete[] data;
        return false;
    }

    webp_data->bytes = data;
    webp_data->size  = fileSize;
    return true;
}

ImageSequenceWebpImpl::ImageSequenceWebpImpl(const std::string& filename) {
    try {
        GeneralLogger::info("Loading WebP image: " + filename, GeneralLogger::STEP);

        WebPDataInit(&m_webpData);

        if (!ReadFileToWebPData(filename, &m_webpData)) {
            throw ImageParseException("Failed to read WebP file: " + filename);
        }

        m_demux = WebPDemux(&m_webpData);
        if (m_demux == nullptr) {
            throw ImageParseException("Failed to create WebP demuxer for: " + filename);
        }

        m_frameCount = WebPDemuxGetI(m_demux, WEBP_FF_FRAME_COUNT);
        if (m_frameCount == 0) {
            GeneralLogger::warning("WebP image has no frames: " + filename);
            m_frameCount = 1;
        }
        GeneralLogger::info("Frame count: " + std::to_string(m_frameCount), GeneralLogger::DETAIL);

        m_width  = WebPDemuxGetI(m_demux, WEBP_FF_CANVAS_WIDTH);
        m_height = WebPDemuxGetI(m_demux, WEBP_FF_CANVAS_HEIGHT);
        GeneralLogger::info("Image dimensions: " + std::to_string(m_width) + "x" + std::to_string(m_height),
                            GeneralLogger::DETAIL);

        if (m_width == 0 || m_height == 0 || m_width > 0x7fffffff || m_height > 0x7fffffff) {
            throw ImageParseException("Invalid WebP image dimensions: " + filename);
        }

        if (!WebPInitDecoderConfig(&m_config)) {
            throw ImageParseException("Failed to initialize WebP decoder for: " + filename);
        }

        m_delays.resize(m_frameCount);
        WebPIterator iter;
        if (WebPDemuxGetFrame(m_demux, 1, &iter)) {
            uint32_t frame_index = 0;
            do {
                m_delays[frame_index] = iter.duration;
                if (m_delays[frame_index] == 0) {
                    m_delays[frame_index] = GIFImage::ImageSequence::DEFAULT_DELAY;
                }
                frame_index++;
            } while (WebPDemuxNextFrame(&iter) && frame_index < m_frameCount);
            WebPDemuxReleaseIterator(&iter);
        } else {
            GeneralLogger::warning("Failed to get WebP frame iterator: " + filename);
            for (uint32_t i = 0; i < m_frameCount; ++i) {
                m_delays[i] = GIFImage::ImageSequence::DEFAULT_DELAY;
            }
        }
    } catch (const std::exception& _) {
        throw;
    } catch (...) {
        throw ImageParseException("Failed to parse WebP image: " + filename);
    }
}

ImageSequenceWebpImpl::~ImageSequenceWebpImpl() {
    if (m_demux) {
        WebPDemuxDelete(m_demux);
        m_demux = nullptr;
    }

    if (m_webpData.bytes) {
        delete[] m_webpData.bytes;
        m_webpData.bytes = nullptr;
        m_webpData.size  = 0;
    }
}

std::vector<PixelBGRA>
ImageSequenceWebpImpl::getFrameBuffer(uint32_t index, uint32_t width, uint32_t height) noexcept {
    try {
        if (index >= m_frameCount) {
            index %= m_frameCount;
        }

        if (width == 0) width = m_width;
        if (height == 0) height = m_height;

        {
            std::lock_guard<std::mutex> lock(m_demuxMutex);

            WebPIterator iter;
            if (!WebPDemuxGetFrame(m_demux, index + 1, &iter)) {
                throw ImageParseException("Failed to get WebP frame at index: " + std::to_string(index));
            }
            const uint32_t srcWidth = iter.width, srcHeight = iter.height;
            if (srcWidth != m_width || srcHeight != m_height) {
                GeneralLogger::warning("WebP frame dimensions do not match: " + std::to_string(srcWidth) + "x" +
                                       std::to_string(srcHeight) + " != " + std::to_string(m_width) + "x" +
                                       std::to_string(m_height));
            }

            m_config.options.use_threads       = 1;
            m_config.output.colorspace         = MODE_BGRA;
            m_config.output.is_external_memory = 1;

            std::vector<PixelBGRA> tempBuffer(srcWidth * srcHeight);

            m_config.output.u.RGBA.rgba   = reinterpret_cast<uint8_t*>(tempBuffer.data());
            m_config.output.u.RGBA.stride = srcWidth * 4;
            m_config.output.u.RGBA.size   = tempBuffer.size() * 4;
            m_config.output.width         = srcWidth;
            m_config.output.height        = srcHeight;

            VP8StatusCode status = WebPDecode(iter.fragment.bytes, iter.fragment.size, &m_config);
            WebPDemuxReleaseIterator(&iter);

            if (status != VP8_STATUS_OK) {
                throw ImageParseException("Failed to decode WebP frame, status: " + std::to_string(status));
            }

            if (width == srcWidth && height == srcHeight) {
                return tempBuffer;
            } else {
                double targetAspect = static_cast<double>(width) / height;
                double imageAspect  = static_cast<double>(srcWidth) / srcHeight;

                Gdiplus::RectF srcRect;
                if (targetAspect > imageAspect) {
                    float newHeight = static_cast<double>(srcWidth) / targetAspect;
                    srcRect         = Gdiplus::RectF(
                        0,
                        (srcHeight - newHeight) / 2,
                        static_cast<float>(srcWidth),
                        newHeight);
                } else {
                    float newWidth = static_cast<double>(srcHeight) * targetAspect;
                    srcRect        = Gdiplus::RectF(
                        (srcWidth - newWidth) / 2,
                        0,
                        newWidth,
                        static_cast<float>(srcHeight));
                }

                Gdiplus::Bitmap srcBitmap(
                    srcWidth,
                    srcHeight,
                    srcWidth * 4,
                    PixelFormat32bppARGB,
                    reinterpret_cast<uint8_t*>(tempBuffer.data()));
                Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);

                Gdiplus::Graphics graphics(&bitmap);
                graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
                graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

                Gdiplus::RectF destRect(0, 0, static_cast<float>(width), static_cast<float>(height));
                graphics.DrawImage(
                    &srcBitmap,
                    destRect,
                    srcRect.X,
                    srcRect.Y,
                    srcRect.Width,
                    srcRect.Height,
                    Gdiplus::UnitPixel);

                Gdiplus::BitmapData bitmapData;
                Gdiplus::Rect lockRect(0, 0, width, height);
                if (bitmap.LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) !=
                    Gdiplus::Ok) {
                    throw ImageParseException("Failed to lock bitmap for pixel extraction.");
                }

                auto* src = static_cast<uint8_t*>(bitmapData.Scan0);
                std::vector<PixelBGRA> pixelData(width * height);

                for (uint32_t y = 0; y < height; ++y) {
                    memcpy(&pixelData[y * width], src + y * bitmapData.Stride, width * 4);
                }
                bitmap.UnlockBits(&bitmapData);
                return pixelData;
            }
        }
    } catch (const std::exception& e) {
        GeneralLogger::error("Error getting WebP frame buffer: " + string(e.what()));
        return {};
    } catch (...) {
        GeneralLogger::error("Unknown error getting WebP frame buffer.");
        return {};
    }
}

wstring
toWstring(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
    wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], size_needed);
    return wstr;
}

ImageSequenceImpl::ImageSequenceImpl(const std::string& filename) {
    try {
        if (!gdiPlusInitializer.initialize()) {
            throw ImageParseException("GDI+ initialization failed.");
        }

        GeneralLogger::info("Loading image: " + filename, GeneralLogger::STEP);

        const auto path = checkFileExists(filename);
        if (path.empty()) {
            throw ImageParseException("File not found: " + filename);
        }

        m_image = new Gdiplus::Image(path.c_str(), false);
        if (m_image->GetLastStatus() != Gdiplus::Ok) {
            throw ImageParseException("Failed to load image: " + filename);
        }

        uint32_t frameCount = m_image->GetFrameCount(&Gdiplus::FrameDimensionTime);
        if (frameCount == 0 || frameCount == 1) {
            GeneralLogger::warning("Image has no frames or only one frame: " + filename);
            frameCount = 1;  // Default to 1 frame if no frames are found
        }
        GeneralLogger::info("Frame count: " + std::to_string(frameCount), GeneralLogger::DETAIL);

        m_delays.resize(frameCount);
        m_width  = m_image->GetWidth();
        m_height = m_image->GetHeight();
        if (m_width == 0 || m_height == 0 || m_width > 0x7fffffff || m_height > 0x7fffffff) {
            throw ImageParseException("Invalid image dimensions: " + filename);
        }
        GeneralLogger::info("Image dimensions: " + std::to_string(m_width) + "x" + std::to_string(m_height),
                            GeneralLogger::DETAIL);

        m_delayPropItem         = nullptr;
        const uint32_t propSize = m_image->GetPropertyItemSize(PropertyTagFrameDelay);
        if (propSize > 0) {
            m_delayPropItem = static_cast<Gdiplus::PropertyItem*>(malloc(propSize));
            m_image->GetPropertyItem(PropertyTagFrameDelay, propSize, m_delayPropItem);
        }

        for (uint32_t i = 0; i < frameCount; ++i) {
            auto status = m_image->SelectActiveFrame(&Gdiplus::FrameDimensionTime, i);
            if (status != Gdiplus::Ok) {
                throw ImageParseException("Failed to select frame: " + filename);
            }
            if (m_width != m_image->GetWidth() || m_height != m_image->GetHeight()) {
                throw ImageParseException("Frame dimensions not consistent: " + filename);
            }
            if (m_delayPropItem != nullptr) {
                m_delays[i] = (static_cast<uint32_t*>(m_delayPropItem->value))[i] * 10;
            } else {
                m_delays[i] = GIFImage::ImageSequence::DEFAULT_DELAY;
            }
        }
    } catch (const std::exception& _) {
        throw;
    } catch (...) {
        throw ImageParseException("Failed to parse image: " + filename);
    }
}

ImageSequenceImpl::~ImageSequenceImpl() {
    delete m_image;
    if (m_delayPropItem) {
        free(m_delayPropItem);
    }
}

std::vector<PixelBGRA>
ImageSequenceImpl::getFrameBuffer(uint32_t index, uint32_t width, uint32_t height) noexcept {
    Gdiplus::Bitmap* bitmap = nullptr;
    try {
        if (index >= m_delays.size()) {
            index %= m_delays.size();
        }
        if (width == 0) width = m_width;
        if (height == 0) height = m_height;

        {
            std::lock_guard<std::mutex> lock(m_imageMutex);

            auto status = m_image->SelectActiveFrame(&Gdiplus::FrameDimensionTime, index);
            if (status != Gdiplus::Ok) {
                throw ImageParseException("Failed to select frame: " + std::to_string(index));
            }

            const auto srcWidth = m_image->GetWidth(), srcHeight = m_image->GetHeight();

            if (srcWidth == 0 || srcHeight == 0) {
                throw ImageParseException("Invalid image dimensions: " + std::to_string(srcWidth) + "x" +
                                          std::to_string(srcHeight));
            }
            if (srcWidth != m_width || srcHeight != m_height) {
                GeneralLogger::warning("Frame dimensions not consistent: " + std::to_string(srcWidth) + "x" +
                                       std::to_string(srcHeight) + " != " + std::to_string(m_width) + "x" +
                                       std::to_string(m_height));
            }

            if (width != srcWidth || height != srcHeight) {
                double targetAspect = static_cast<double>(width) / height;
                double imageAspect  = static_cast<double>(srcWidth) / srcHeight;

                Gdiplus::RectF srcRect;
                if (targetAspect > imageAspect) {
                    float newHeight = static_cast<float>(srcWidth) / targetAspect;
                    srcRect         = Gdiplus::RectF(0, (srcHeight - newHeight) / 2, static_cast<float>(srcWidth), newHeight);
                } else {
                    float newWidth = static_cast<float>(srcHeight) * targetAspect;
                    srcRect        = Gdiplus::RectF((srcWidth - newWidth) / 2, 0, newWidth, static_cast<float>(srcHeight));
                }

                bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
                Gdiplus::Graphics graphics(bitmap);

                graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
                graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

                Gdiplus::RectF destRect(0, 0, static_cast<float>(width), static_cast<float>(height));
                graphics.DrawImage(
                    m_image,
                    destRect,
                    srcRect.X,
                    srcRect.Y,
                    srcRect.Width,
                    srcRect.Height,
                    Gdiplus::UnitPixel);
                if (graphics.GetLastStatus() != Gdiplus::Ok) {
                    throw ImageParseException("Failed to draw image. Error code: " +
                                              std::to_string(graphics.GetLastStatus()));
                }
            } else {
                width = srcWidth, height = srcHeight;
                bitmap = new Gdiplus::Bitmap(srcWidth, srcHeight, PixelFormat32bppARGB);
                Gdiplus::Graphics graphics(bitmap);

                graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
                graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeNone);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeNone);
                graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

                graphics.DrawImage(m_image, 0, 0, srcWidth, srcHeight);
                if (graphics.GetLastStatus() != Gdiplus::Ok) {
                    throw ImageParseException("Failed to draw image. Error code: " +
                                              std::to_string(graphics.GetLastStatus()));
                }
            }
        }
        Gdiplus::BitmapData bitmapData;
        Gdiplus::Rect lockRect(0, 0, width, height);
        if (bitmap->LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) != Gdiplus::Ok) {
            throw ImageParseException("Failed to lock bitmap for pixel extraction.");
        }

        std::vector<PixelBGRA> pixelData(width * height);
        const auto* src = static_cast<uint8_t*>(bitmapData.Scan0);
        for (uint32_t y = 0; y < height; ++y) {
            memcpy(&pixelData[y * width], src + y * bitmapData.Stride, width * 4);
        }

        bitmap->UnlockBits(&bitmapData);
        delete bitmap;
        return pixelData;
    } catch (const std::exception& e) {
        GeneralLogger::error("Error getting frame buffer: " + string(e.what()));
        delete bitmap;
        return {};
    } catch (...) {
        GeneralLogger::error("Unknown error getting frame buffer.");
        delete bitmap;
        return {};
    }
}

bool
GIFImage::ImageSequence::initDecoder(const char*) noexcept {
    return gdiPlusInitializer.initialize();
}

bool
GIFImage::ImageSequence::drawText(vector<PixelBGRA>& buffer,
                                  const uint32_t width,
                                  const uint32_t height,
                                  const string& text,
                                  const PixelBGRA& textBackgroundColor,
                                  const PixelBGRA& textForegroundColor,
                                  const double textHeightRatio,
                                  const double textPadding,
                                  const uint32_t x,
                                  const uint32_t y,
                                  const string& fontFamilyPara) noexcept {
    try {
        if (text.empty()) return true;  // do nothing
        if (buffer.empty() || width == 0 || height == 0 || x >= width || y >= height) {
            throw ImageParseException("Invalid buffer or dimensions for drawing text.");
        }
        if (buffer.size() != width * height) {
            throw ImageParseException("Buffer size does not match dimensions: " + std::to_string(buffer.size()) +
                                      " != " + std::to_string(width * height));
        }

        Gdiplus::FontFamily fontFamily(toWstring(fontFamilyPara).c_str());
        if (fontFamily.GetLastStatus() != Gdiplus::Ok) {
            if (fontFamilyPara == FALLBACK_FONT) {
                throw ImageParseException("Failed to create font.");
            }
            GeneralLogger::warning("Failed to create font family: " + fontFamilyPara + ". Falling back to " +
                                   FALLBACK_FONT);
            return drawText(buffer,
                            width,
                            height,
                            text,
                            textBackgroundColor,
                            textForegroundColor,
                            textHeightRatio,
                            textPadding,
                            x,
                            y,
                            FALLBACK_FONT);
        }

        auto* bgraBuffer = reinterpret_cast<uint8_t*>(buffer.data());
        Gdiplus::Bitmap bitmap(width, height, width * 4, PixelFormat32bppARGB, bgraBuffer);
        auto status = bitmap.GetLastStatus();
        if (status != Gdiplus::Ok) {
            throw ImageParseException("Failed to read from pixel buffer. Error code: " + std::to_string(status));
        }

        Gdiplus::Graphics graphics(&bitmap);
        if (graphics.GetLastStatus() != Gdiplus::Ok) {
            throw ImageParseException("Failed to create canvas. Error code: " +
                                      std::to_string(graphics.GetLastStatus()));
        }

        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        const auto textBoxHeight = static_cast<uint32_t>(height * textHeightRatio);
        Gdiplus::Font font(&fontFamily,
                           static_cast<float>(textBoxHeight) * (1 - textPadding * 2),
                           Gdiplus::FontStyleRegular,
                           Gdiplus::UnitPixel);
        const Gdiplus::SolidBrush textBrush(
            Gdiplus::Color(textForegroundColor.a, textForegroundColor.r, textForegroundColor.g, textForegroundColor.b));
        const Gdiplus::PointF point(x + static_cast<float>(textBoxHeight) * textPadding,
                                    y + static_cast<float>(textBoxHeight) * textPadding);

        // Measure the size of the text
        Gdiplus::RectF textBounds;
        graphics.MeasureString(toWstring(text).c_str(), -1, &font, point, &textBounds);

        // Create a background rectangle that matches the text bounds
        Gdiplus::RectF backgroundRect(x,
                                      y,
                                      x + textBounds.Width + (textBoxHeight * textPadding * 2),
                                      y + textBounds.Height + (textBoxHeight * textPadding * 2));
        Gdiplus::SolidBrush backgroundBrush(
            Gdiplus::Color(textBackgroundColor.a, textBackgroundColor.r, textBackgroundColor.g, textBackgroundColor.b));
        graphics.FillRectangle(&backgroundBrush, backgroundRect);
        if (graphics.GetLastStatus() != Gdiplus::Ok) {
            throw ImageParseException("Failed to fill background rectangle. Error code: " +
                                      std::to_string(graphics.GetLastStatus()));
        }

        // Draw the text
        graphics.DrawString(toWstring(text).c_str(), -1, &font, point, nullptr, &textBrush);
        Gdiplus::Status drawStatus = graphics.GetLastStatus();
        if (drawStatus != Gdiplus::Ok) {
            throw ImageParseException("Failed to draw text. Error code: " + std::to_string(drawStatus));
        }

        return true;
    } catch (const std::exception& e) {
        GeneralLogger::error("Error drawing text: " + string(e.what()));
        return false;
    } catch (...) {
        GeneralLogger::error("Unknown error drawing text.");
        return false;
    }
}

vector<PixelBGRA>
GIFImage::ImageSequence::resizeCover(const vector<PixelBGRA>& buffer,
                                     const uint32_t origWidth,
                                     const uint32_t origHeight,
                                     const uint32_t targetWidth,
                                     const uint32_t targetHeight) noexcept {
    try {
        if (buffer.empty() || origWidth == 0 || origHeight == 0 || targetWidth == 0 || targetHeight == 0) {
            throw ImageParseException("Invalid buffer or dimensions for resizing.");
        }
        if (buffer.size() != origWidth * origHeight) {
            throw ImageParseException("Buffer size does not match original dimensions: " +
                                      std::to_string(buffer.size()) + " != " + std::to_string(origWidth * origHeight));
        }
        if (targetWidth == origWidth && targetHeight == origHeight) {
            return buffer;  // No resizing needed
        }

        double targetAspect = static_cast<double>(targetWidth) / targetHeight;
        double imageAspect  = static_cast<double>(origWidth) / origHeight;

        Gdiplus::RectF srcRect;
        if (targetAspect > imageAspect) {
            float newHeight = static_cast<float>(origWidth) / targetAspect;
            srcRect         = Gdiplus::RectF(0, (origHeight - newHeight) / 2, static_cast<float>(origWidth), newHeight);
        } else {
            float newWidth = static_cast<float>(origHeight) * targetAspect;
            srcRect        = Gdiplus::RectF((origWidth - newWidth) / 2, 0, newWidth, static_cast<float>(origHeight));
        }

        auto bitmap = Gdiplus::Bitmap(targetWidth, targetHeight, PixelFormat32bppARGB);
        Gdiplus::Graphics graphics(&bitmap);

        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

        Gdiplus::RectF destRect(0, 0, static_cast<float>(targetWidth), static_cast<float>(targetHeight));
        Gdiplus::Bitmap srcBitmap(origWidth, origHeight, origWidth * 4, PixelFormat32bppARGB, reinterpret_cast<uint8_t*>(const_cast<PixelBGRA*>(buffer.data())));
        graphics.DrawImage(
            &srcBitmap,
            destRect,
            srcRect.X,
            srcRect.Y,
            srcRect.Width,
            srcRect.Height,
            Gdiplus::UnitPixel);
        if (graphics.GetLastStatus() != Gdiplus::Ok) {
            throw ImageParseException("Failed to draw image. Error code: " +
                                      std::to_string(graphics.GetLastStatus()));
        }
        Gdiplus::BitmapData bitmapData;
        Gdiplus::Rect lockRect(0, 0, targetWidth, targetHeight);
        if (bitmap.LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) != Gdiplus::Ok) {
            throw ImageParseException("Failed to lock bitmap for pixel extraction.");
        }
        std::vector<PixelBGRA> pixelData(targetWidth * targetHeight);
        const auto* src = static_cast<uint8_t*>(bitmapData.Scan0);
        for (uint32_t y = 0; y < targetHeight; ++y) {
            memcpy(&pixelData[y * targetWidth], src + y * bitmapData.Stride, targetWidth * 4);
        }
        bitmap.UnlockBits(&bitmapData);
        return pixelData;
    } catch (const std::exception& e) {
        GeneralLogger::error("Error resizing image: " + string(e.what()));
        return {};
    } catch (...) {
        GeneralLogger::error("Unknown error resizing image.");
        return {};
    }
}

bool
GIFImage::ImageSequence::drawMark(vector<PixelBGRA>& buffer,
                                  const uint32_t width,
                                  const uint32_t height,
                                  const vector<PixelBGRA>& markBuffer,
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
        GeneralLogger::error("Error drawing mark: " + string(e.what()));
    } catch (...) {
        GeneralLogger::error("Unknown error drawing mark.");
    }
    return false;
}

static uint32_t
base64DecodeChar(char ch) {
    // if (ch >= 'a') return (ch - 'a' + 26);
    // if (ch >= 'A') return (ch - 'A');
    // if (ch >= '0') return (ch - '0' + 52);
    // if (ch == '+') return 62;
    // if (ch == '/') return 63;
    // return 0;
    static const std::unordered_map<char, uint32_t> charMap{
        {'A', 0},
        {'B', 1},
        {'C', 2},
        {'D', 3},
        {'E', 4},
        {'F', 5},
        {'G', 6},
        {'H', 7},
        {'I', 8},
        {'J', 9},
        {'K', 10},
        {'L', 11},
        {'M', 12},
        {'N', 13},
        {'O', 14},
        {'P', 15},
        {'Q', 16},
        {'R', 17},
        {'S', 18},
        {'T', 19},
        {'U', 20},
        {'V', 21},
        {'W', 22},
        {'X', 23},
        {'Y', 24},
        {'Z', 25},
        {'a', 26},
        {'b', 27},
        {'c', 28},
        {'d', 29},
        {'e', 30},
        {'f', 31},
        {'g', 32},
        {'h', 33},
        {'i', 34},
        {'j', 35},
        {'k', 36},
        {'l', 37},
        {'m', 38},
        {'n', 39},
        {'o', 40},
        {'p', 41},
        {'q', 42},
        {'r', 43},
        {'s', 44},
        {'t', 45},
        {'u', 46},
        {'v', 47},
        {'w', 48},
        {'x', 49},
        {'y', 50},
        {'z', 51},
        {'0', 52},
        {'1', 53},
        {'2', 54},
        {'3', 55},
        {'4', 56},
        {'5', 57},
        {'6', 58},
        {'7', 59},
        {'8', 60},
        {'9', 61},
        {'+', 62},
        {'/', 63},  // standard base64
        {'-', 62},
        {'_', 63},  // URL-safe base64
        {'=', 0},
        {' ', 0},
        {'\n', 0},
        {'\r', 0},
        {'\t', 0}  // padding and whitespace
    };
    const auto it = charMap.find(ch);
    if (it != charMap.end()) {
        return it->second;
    } else {
        throw ImageParseException("Invalid base64 character: " + std::string(1, ch));
    }
}

static vector<uint8_t>
base64Decode(const string& base64) {
    const auto pos = base64.find("base64,");
    if (pos == string::npos) {
        throw ImageParseException("Invalid base64 string: missing 'base64,' prefix.");
    }
    const auto stringLen  = base64.size() - pos - 7;
    const auto base64Data = std::string_view(base64.data() + pos + 7, stringLen);
    if (stringLen % 4 != 0) {
        throw ImageParseException("Invalid base64 data length: " + std::to_string(stringLen));
    }
    size_t padding = 0;
    if (base64Data.back() == '=') {
        if (base64Data[stringLen - 1] == '=') {
            padding++;
            if (base64Data[stringLen - 2] == '=') {
                padding++;
                if (base64Data[stringLen - 3] == '=') {
                    throw ImageParseException("Invalid base64 string: too many padding characters.");
                }
            }
        }
    }

    vector<uint8_t> decodedData;
    const auto dataLen = (stringLen / 4) * 3 - padding;
    decodedData.resize(dataLen);
    for (size_t i = 0, j = 0; i < stringLen; i += 4) {
        const auto a = base64DecodeChar(base64Data[i]);
        const auto b = base64DecodeChar(base64Data[i + 1]);
        const auto c = base64DecodeChar(base64Data[i + 2]);
        const auto d = base64DecodeChar(base64Data[i + 3]);

        decodedData[j++] = (a << 2) | (b >> 4);
        if (j < dataLen) decodedData[j++] = ((b & 0x0F) << 4) | (c >> 2);
        if (j < dataLen) decodedData[j++] = ((c & 0x03) << 6) | d;
    }
    return decodedData;
}

vector<PixelBGRA>
GIFImage::ImageSequence::parseBase64(const string& base64) noexcept {
    vector<PixelBGRA> result;

    HGLOBAL hGlobal         = nullptr;
    IStream* pStream        = nullptr;
    Gdiplus::Bitmap* bitmap = nullptr;

    const auto defer = [&]() {
        if (hGlobal) {
            if (GlobalFlags(hGlobal) & GMEM_LOCKCOUNT) {
                GlobalUnlock(hGlobal);
            }
            GlobalFree(hGlobal);
        }
        if (pStream) {
            pStream->Release();
        }
        delete bitmap;
    };

    try {
        if (base64.empty()) {
            throw ImageParseException("Base64 string is empty.");
        }
        const auto decodedData = base64Decode(base64);
        if (decodedData.empty()) {
            throw ImageParseException("Failed to decode base64 string.");
        }

        hGlobal = GlobalAlloc(GMEM_MOVEABLE, decodedData.size());
        if (!hGlobal) {
            throw ImageParseException("Failed to allocate global memory for base64 data.");
        }

        void* pData = GlobalLock(hGlobal);
        if (!pData) {
            throw ImageParseException("Failed to lock global memory for base64 data.");
        }
        memcpy(pData, decodedData.data(), decodedData.size());
        GlobalUnlock(hGlobal);

        pStream = nullptr;
        if (CreateStreamOnHGlobal(hGlobal, TRUE, &pStream) != S_OK) {
            throw ImageParseException("Failed to create stream from global memory.");
        }

        bitmap = Gdiplus::Bitmap::FromStream(pStream);
        if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok) {
            throw ImageParseException("Failed to create bitmap from base64 data.");
        }

        uint32_t width  = bitmap->GetWidth();
        uint32_t height = bitmap->GetHeight();
        result.resize(width * height);

        Gdiplus::BitmapData bitmapData;
        Gdiplus::Rect lockRect(0, 0, width, height);
        if (bitmap->LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) != Gdiplus::Ok) {
            throw ImageParseException("Failed to lock bitmap for pixel extraction.");
        }

        auto* src = static_cast<uint8_t*>(bitmapData.Scan0);
        for (uint32_t y = 0; y < height; ++y) {
            memcpy(&result[y * width], src + y * bitmapData.Stride, width * 4);
        }
        bitmap->UnlockBits(&bitmapData);
    } catch (const std::exception& e) {
        GeneralLogger::error("Error parsing base64 image: " + string(e.what()));
    } catch (...) {
        GeneralLogger::error("Unknown error parsing base64 image.");
    }
    defer();
    return result;
}

#endif  // IMSQ_USE_GDIPLUS