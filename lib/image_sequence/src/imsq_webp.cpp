#include <webp/decode.h>
#include <webp/demux.h>

#include <exception>
#include <filesystem>
#include <fstream>
#include <mutex>

#include "file_utils.h"
#include "imsq.h"
#include "imsq_exception.h"
#include "log.h"

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

inline bool
readFileToWebPData(const std::string& filename, WebPData* webp_data) {
    const auto localized = NaiveIO::checkFileExists(filename);
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

        if (!readFileToWebPData(filename, &m_webpData)) {
            throw ImageParseException("Failed to read WebP file: " + filename);
        }

        m_demux = WebPDemux(&m_webpData);
        if (m_demux == nullptr) {
            throw ImageParseException("Failed to create WebP demuxer for: " + filename);
        }

        m_frameCount = WebPDemuxGetI(m_demux, WEBP_FF_FRAME_COUNT);
        if (m_frameCount == 0) {
            GeneralLogger::warn("WebP image has no frames: " + filename);
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
            GeneralLogger::warn("Failed to get WebP frame iterator: " + filename);
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
                GeneralLogger::warn("WebP frame dimensions do not match: " + std::to_string(srcWidth) + "x" +
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
                return resizeCover(tempBuffer, srcWidth, srcHeight, width, height);
            }
        }
    } catch (const std::exception& e) {
        GeneralLogger::error("Error getting WebP frame buffer: " + std::string(e.what()));
        return {};
    } catch (...) {
        GeneralLogger::error("Unknown error getting WebP frame buffer.");
        return {};
    }
}