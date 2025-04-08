#ifndef _WIN32
#error "Gdiplus is only available on Windows."
#endif

#include <windows.h>
//
#include <gdiplus.h>
#include <webp/decode.h>
#include <webp/demux.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

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

    std::vector<u32>&
    getDelays() noexcept override {
        return m_delays;
    }

    [[nodiscard]] u32
    getFrameCount() const noexcept override {
        return static_cast<u32>(m_delays.size());
    }

    [[nodiscard]] u32
    getWidth() const noexcept override {
        return m_width;
    }

    [[nodiscard]] u32
    getHeight() const noexcept override {
        return m_height;
    }

    std::vector<PixelBGRA>
    getFrameBuffer(u32 index, u32 width, u32 height, bool ensureAccurate) noexcept override;

   private:
    Gdiplus::Image* m_image                = nullptr;
    Gdiplus::PropertyItem* m_delayPropItem = nullptr;
    std::vector<u32> m_delays;
    u32 m_width  = 0;
    u32 m_height = 0;
    std::mutex m_imageMutex;
};

class ImageSequenceWebpImpl : public GIFImage::ImageSequence {
   public:
    explicit ImageSequenceWebpImpl(const std::string& filename);
    ~ImageSequenceWebpImpl() override;

    std::vector<u32>&
    getDelays() noexcept override {
        return m_delays;
    }
    [[nodiscard]] u32
    getFrameCount() const noexcept override {
        return m_frameCount;
    }
    [[nodiscard]] u32
    getWidth() const noexcept override {
        return m_width;
    }
    [[nodiscard]] u32
    getHeight() const noexcept override {
        return m_height;
    }
    std::vector<PixelBGRA>
    getFrameBuffer(u32 index, u32 width, u32 height, bool ensureAccurate) noexcept override;

   private:
    WebPData m_webpData{};
    WebPDemuxer* m_demux = nullptr;
    WebPDecoderConfig m_config{};
    std::vector<u32> m_delays;
    u32 m_frameCount = 0;
    u32 m_width      = 0;
    u32 m_height     = 0;
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
    } catch (const ImageParseException& e) {
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

    u8* data = new u8[fileSize];
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
            u32 frame_index = 0;
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
            for (u32 i = 0; i < m_frameCount; ++i) {
                m_delays[i] = GIFImage::ImageSequence::DEFAULT_DELAY;
            }
        }
    } catch (const ImageParseException& _) {
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
ImageSequenceWebpImpl::getFrameBuffer(u32 index, u32 width, u32 height, bool ensureAccurate) noexcept {
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
            const u32 srcWidth = iter.width, srcHeight = iter.height;
            if (srcWidth != m_width || srcHeight != m_height) {
                GeneralLogger::warning("WebP frame dimensions do not match: " + std::to_string(srcWidth) + "x" +
                                       std::to_string(srcHeight) + " != " + std::to_string(m_width) + "x" +
                                       std::to_string(m_height));
            }

            m_config.options.use_threads       = 1;
            m_config.output.colorspace         = MODE_BGRA;
            m_config.output.is_external_memory = 1;

            std::vector<PixelBGRA> tempBuffer(srcWidth * srcHeight);

            m_config.output.u.RGBA.rgba   = reinterpret_cast<u8*>(tempBuffer.data());
            m_config.output.u.RGBA.stride = srcWidth * 4;
            m_config.output.u.RGBA.size   = tempBuffer.size() * 4;
            m_config.output.width         = srcWidth;
            m_config.output.height        = srcHeight;

            VP8StatusCode status = WebPDecode(iter.fragment.bytes, iter.fragment.size, &m_config);
            WebPDemuxReleaseIterator(&iter);

            if (status != VP8_STATUS_OK) {
                throw ImageParseException("Failed to decode WebP frame, status: " + std::to_string(status));
            }

            if (ensureAccurate || (width == srcWidth && height == srcHeight)) {
                return tempBuffer;
            } else {
                double targetAspect = static_cast<double>(width) / height;
                double imageAspect  = static_cast<double>(srcWidth) / srcHeight;

                Gdiplus::RectF srcRect;
                if (targetAspect > imageAspect) {
                    float newHeight = static_cast<double>(srcWidth) / targetAspect;
                    srcRect = Gdiplus::RectF(0, (srcHeight - newHeight) / 2, static_cast<float>(srcWidth), newHeight);
                } else {
                    float newWidth = static_cast<double>(srcHeight) * targetAspect;
                    srcRect = Gdiplus::RectF((srcWidth - newWidth) / 2, 0, newWidth, static_cast<float>(srcHeight));
                }

                Gdiplus::Bitmap srcBitmap(
                    srcWidth, srcHeight, srcWidth * 4, PixelFormat32bppARGB, reinterpret_cast<u8*>(tempBuffer.data()));
                Gdiplus::Bitmap bitmap(width, height, PixelFormat32bppARGB);

                Gdiplus::Graphics graphics(&bitmap);
                graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
                graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

                Gdiplus::RectF destRect(0, 0, static_cast<float>(width), static_cast<float>(height));
                graphics.DrawImage(
                    &srcBitmap, destRect, srcRect.X, srcRect.Y, srcRect.Width, srcRect.Height, Gdiplus::UnitPixel);

                Gdiplus::BitmapData bitmapData;
                Gdiplus::Rect lockRect(0, 0, width, height);
                if (bitmap.LockBits(&lockRect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData) !=
                    Gdiplus::Ok) {
                    throw ImageParseException("Failed to lock bitmap for pixel extraction.");
                }

                u8* src = static_cast<u8*>(bitmapData.Scan0);
                std::vector<PixelBGRA> pixelData(width * height);

                for (u32 y = 0; y < height; ++y) {
                    memcpy(&pixelData[y * width], src + y * bitmapData.Stride, width * 4);
                }
                bitmap.UnlockBits(&bitmapData);
                return pixelData;
            }
        }
    } catch (const ImageParseException& e) {
        GeneralLogger::error("Error getting WebP frame buffer: " + string(e.what()));
        return {};
    } catch (...) {
        GeneralLogger::error("Unknown error getting WebP frame buffer.");
        return {};
    }
}

class GdiPlusInitializer {
   private:
    ULONG_PTR token  = 0;
    bool initialized = false;

   public:
    GdiPlusInitializer() = default;

    bool
    initialize() {
        if (initialized) return true;
        if (token != 0) return false;
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::Status status = Gdiplus::GdiplusStartup(&token, &input, nullptr);
        initialized            = (status == Gdiplus::Ok);
        return initialized;
    }

    ~GdiPlusInitializer() {
        if (initialized) {
            Gdiplus::GdiplusShutdown(token);
        }
    }

    [[nodiscard]] bool
    isInitialized() const {
        return initialized;
    }
};

static GdiPlusInitializer gdiPlusInitializer;

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

        u32 frameCount = m_image->GetFrameCount(&Gdiplus::FrameDimensionTime);
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

        m_delayPropItem    = nullptr;
        const u32 propSize = m_image->GetPropertyItemSize(PropertyTagFrameDelay);
        if (propSize > 0) {
            m_delayPropItem = static_cast<Gdiplus::PropertyItem*>(malloc(propSize));
            m_image->GetPropertyItem(PropertyTagFrameDelay, propSize, m_delayPropItem);
        }

        for (u32 i = 0; i < frameCount; ++i) {
            auto status = m_image->SelectActiveFrame(&Gdiplus::FrameDimensionTime, i);
            if (status != Gdiplus::Ok) {
                throw ImageParseException("Failed to select frame: " + filename);
            }
            if (m_width != m_image->GetWidth() || m_height != m_image->GetHeight()) {
                throw ImageParseException("Frame dimensions not consistent: " + filename);
            }
            if (m_delayPropItem != nullptr) {
                m_delays[i] = (static_cast<u32*>(m_delayPropItem->value))[i] * 10;
            } else {
                m_delays[i] = GIFImage::ImageSequence::DEFAULT_DELAY;
            }
        }
    } catch (const ImageParseException& _) {
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
ImageSequenceImpl::getFrameBuffer(u32 index, u32 width, u32 height, bool ensureAccurate) noexcept {
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

            if (!ensureAccurate) {
                double targetAspect = static_cast<double>(width) / height;
                double imageAspect  = static_cast<double>(srcWidth) / srcHeight;

                Gdiplus::RectF srcRect;
                if (targetAspect > imageAspect) {
                    float newHeight = static_cast<float>(srcWidth) / targetAspect;
                    srcRect = Gdiplus::RectF(0, (srcHeight - newHeight) / 2, static_cast<float>(srcWidth), newHeight);
                } else {
                    float newWidth = static_cast<float>(srcHeight) * targetAspect;
                    srcRect = Gdiplus::RectF((srcWidth - newWidth) / 2, 0, newWidth, static_cast<float>(srcHeight));
                }

                bitmap = new Gdiplus::Bitmap(width, height, PixelFormat32bppARGB);
                Gdiplus::Graphics graphics(bitmap);

                graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
                graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
                graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

                Gdiplus::RectF destRect(0, 0, static_cast<float>(width), static_cast<float>(height));
                graphics.DrawImage(
                    m_image, destRect, srcRect.X, srcRect.Y, srcRect.Width, srcRect.Height, Gdiplus::UnitPixel);
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
        u8* src = static_cast<u8*>(bitmapData.Scan0);
        for (u32 y = 0; y < height; ++y) {
            memcpy(&pixelData[y * width], src + y * bitmapData.Stride, width * 4);
        }

        bitmap->UnlockBits(&bitmapData);
        delete bitmap;
        return pixelData;
    } catch (const ImageParseException& e) {
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
GIFImage::ImageSequence::drawText(std::span<PixelBGRA>& buffer,
                                  u32 width,
                                  u32 height,
                                  const string& text,
                                  const PixelBGRA& textBackgroundColor,
                                  const PixelBGRA& textForegroundColor,
                                  float textHeightRatio,
                                  float textPadding,
                                  const string& fontFamilyPara) noexcept {
    try {
        if (buffer.empty() || width == 0 || height == 0) {
            throw ImageParseException("Invalid buffer or dimensions for drawing text.");
        }
        if (buffer.size() != width * height) {
            throw ImageParseException("Buffer size does not match dimensions: " + std::to_string(buffer.size()) +
                                      " != " + std::to_string(width * height));
        }

        Gdiplus::FontFamily fontFamily(toWstring(fontFamilyPara.c_str()).c_str());
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
                            FALLBACK_FONT);
        }

        u8* bgraBuffer = reinterpret_cast<u8*>(buffer.data());
        Gdiplus::Bitmap bitmap(width, height, width * 4, PixelFormat32bppARGB, bgraBuffer);
        auto status = bitmap.GetLastStatus();
        if (status != Gdiplus::Ok) {
            throw ImageParseException("Failed to read from pixel buffer. Error code: " + std::to_string(status));
            return false;
        }

        Gdiplus::Graphics graphics(&bitmap);
        if (graphics.GetLastStatus() != Gdiplus::Ok) {
            throw ImageParseException("Failed to create canvas. Error code: " +
                                      std::to_string(graphics.GetLastStatus()));
            return false;
        }

        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);

        u32 textBoxHeight = static_cast<u32>(height * textHeightRatio);
        Gdiplus::Font font(&fontFamily,
                           static_cast<float>(textBoxHeight) * (1 - textPadding * 2),
                           Gdiplus::FontStyleRegular,
                           Gdiplus::UnitPixel);
        Gdiplus::SolidBrush textBrush(
            Gdiplus::Color(textForegroundColor.a, textForegroundColor.r, textForegroundColor.g, textForegroundColor.b));
        Gdiplus::PointF point(static_cast<float>(textBoxHeight) * textPadding,
                              static_cast<float>(textBoxHeight) * textPadding);

        // Measure the size of the text
        Gdiplus::RectF textBounds;
        graphics.MeasureString(toWstring(text).c_str(), -1, &font, point, &textBounds);

        // Create a background rectangle that matches the text bounds
        Gdiplus::RectF backgroundRect(0,
                                      0,
                                      textBounds.Width + (textBoxHeight * textPadding * 2),
                                      textBounds.Height + (textBoxHeight * textPadding * 2));
        Gdiplus::SolidBrush backgroundBrush(
            Gdiplus::Color(textBackgroundColor.a, textBackgroundColor.r, textBackgroundColor.g, textBackgroundColor.b));
        graphics.FillRectangle(&backgroundBrush, backgroundRect);
        if (graphics.GetLastStatus() != Gdiplus::Ok) {
            throw ImageParseException("Failed to fill background rectangle. Error code: " +
                                      std::to_string(graphics.GetLastStatus()));
            return false;
        }

        // Draw the text
        graphics.DrawString(toWstring(text).c_str(), -1, &font, point, nullptr, &textBrush);
        Gdiplus::Status drawStatus = graphics.GetLastStatus();
        if (drawStatus != Gdiplus::Ok) {
            throw ImageParseException("Failed to draw text. Error code: " + std::to_string(drawStatus));
            return false;
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