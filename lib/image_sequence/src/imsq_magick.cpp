#if 1
#error "unimplemented"
#else

#include <Magick++.h>

#include <cstdarg>
#include <exception>
#include <filesystem>
#include <fstream>

#include "imsq.h"
#include "log.h"
#include "path.h"

using std::string, std::vector, std::wstring;

class ImageSequenceImpl : public GIFImage::ImageSequence {
  public:
    ImageSequenceImpl(const std::string& filename);

    ~ImageSequenceImpl();

    std::vector<u32>&
    getDelays() noexcept override {
        return m_delays;
    }

    u32
    getFrameCount() const noexcept override {
        return m_delays.size();
    }

    u32
    getWidth() const noexcept override {
        return m_width;
    }

    u32
    getHeight() const noexcept override {
        return m_height;
    }

    std::vector<PixelBGRA>
    getFrameBuffer(u32 index, u32 width, u32 height, bool ensureAccurate) noexcept override;

  private:
    vector<Magick::Image> m_images;
    std::vector<u32> m_delays;
    u32 m_width  = 0;
    u32 m_height = 0;
};

bool
GIFImage::ImageSequence::initDecoder(const char* arg0) noexcept {
    static bool initialized = false;
    if (initialized) return true;
    initialized = true;

    Magick::InitializeMagick(arg0);
    return true;
}

class ImageParseException final : public std::exception {
  public:
    explicit ImageParseException(const std::string&& message) : m_msg(message) {}

    const char*
    what() const noexcept override {
        return m_msg.c_str();
    }

  private:
    std::string m_msg;
};

GIFImage::ImageSequenceRef
GIFImage::ImageSequence::read(const std::string& filename) noexcept {
    try {
        return std::make_unique<ImageSequenceImpl>(filename);
    } catch (const ImageParseException& e) {
        GeneralLogger::error("Error reading image sequence: " + string(e.what()));
        return nullptr;
    }
}

ImageSequenceImpl::ImageSequenceImpl(const std::string& filename) {
    try {
        vector<u8> data;
        const auto filePath = std::filesystem::path(localizePath(filename));
        if (!std::filesystem::exists(filePath)) {
            throw ImageParseException("File not found: " + filename);
        }
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            throw ImageParseException("Failed to open file: " + filename);
        }
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        data.resize(fileSize);
        file.read(reinterpret_cast<char*>(data.data()), fileSize);
        if (!file) {
            throw ImageParseException("Failed to read file: " + filename);
        }
        file.close();
        Magick::Blob blob(data.data(), data.size());
        Magick::readImages(&m_images, blob);
        if (m_images.empty()) {
            throw ImageParseException("No images found in file: " + filename);
        } else if (m_images.size() == 1ull) {
            GeneralLogger::warning("Only one image found in file: " + filename);
        }
        m_width  = m_images[0].columns();
        m_height = m_images[0].rows();

        m_delays.resize(m_images.size(), GIFImage::ImageSequence::DEFAULT_DELAY);
        for (size_t i = 0; i < m_images.size(); ++i) {
            auto& image      = m_images[i];
            const auto delay = image.animationDelay();
            if (delay > 0) {
                m_delays[i] = delay * 10;  // Convert to milliseconds
            } else {
                m_delays[i] = GIFImage::ImageSequence::DEFAULT_DELAY;
            }
        }
    } catch (const Magick::Exception& e) {
        throw ImageParseException("Magick++ error: " + string(e.what()));
    } catch (const std::exception& e) {
        throw ImageParseException(e.what());
    } catch (...) {
        throw ImageParseException("Unknown error occurred while reading image: " + filename);
    }
}

std::vector<PixelBGRA>
ImageSequenceImpl::getFrameBuffer(u32 index, u32 width, u32 height, bool ensureAccurate) noexcept {
    if (index >= m_images.size()) {
        index %= m_images.size();
    }
    auto& image = m_images[index];
    if (width == 0) width = m_width;
    if (height == 0) height = m_height;

    try {
        if (width != image.columns() || height != image.rows()) {
            double targetRatio = static_cast<double>(width) / height;
            double imageRatio  = static_cast<double>(image.columns()) / image.rows();

            size_t scaleWidth, scaleHeight;
            if (imageRatio > targetRatio) {
                scaleHeight = height;
                scaleWidth  = static_cast<size_t>(height * imageRatio);
            } else {
                scaleWidth  = width;
                scaleHeight = static_cast<size_t>(width / imageRatio);
            }

            image.resize(Magick::Geometry(scaleWidth, scaleHeight));

            size_t x = (scaleWidth - width) / 2;
            size_t y = (scaleHeight - height) / 2;
            image.crop(Magick::Geometry(width, height, x, y));
            image.page(Magick::Geometry(width, height, 0, 0));  // 重置页面几何信息
        }

        image.type(Magick::TrueColorMatteType);

        std::vector<u8> buffer(width * height * 4);

        const Magick::PixelPacket* pixels = image.getConstPixels(0, 0, width, height);

        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                const auto& pixel = pixels[y * width + x];
                size_t idx        = (y * width + x) * 4;
                buffer[idx + 2]   = static_cast<u8>(pixel.red);
                buffer[idx + 1]   = static_cast<u8>(pixel.green);
                buffer[idx + 0]   = static_cast<u8>(pixel.blue);
                buffer[idx + 3]   = static_cast<u8>(pixel.opacity);
            }
        }

        return buffer;
    } catch (...) {
        return std::vector<u8>();
    }
}

#endif  // 1