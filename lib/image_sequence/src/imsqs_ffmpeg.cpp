#ifdef IMSQ_USE_FFMPEG

#include <exception>
#include <string>
#include <vector>

#include "imsq_stream.h"
#include "log.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

using std::string, std::vector;

using namespace GIFImage;

bool
ImageSequenceStream::initDecoder(const char*) noexcept {
    return true;
}

class ImageParseException final : public std::exception {
  public:
    explicit ImageParseException(const string&& message) : m_msg(message) {}

    [[nodiscard]] const char*
    what() const noexcept override {
        return m_msg.c_str();
    }

  private:
    string m_msg;
};

class EOFException : public std::exception {};

class ImageSequenceStreamImpl : public ImageSequenceStream {
  public:
    explicit ImageSequenceStreamImpl(const string& filename);

    ~ImageSequenceStreamImpl() noexcept override;

    [[nodiscard]] FrameRef
    getNextFrame() noexcept override;

    [[nodiscard]] bool
    isEndOfStream() const noexcept override {
        return m_eof;
    }

  private:
    AVFormatContext* m_formatCtx     = nullptr;
    AVCodecContext* m_codecCtx       = nullptr;
    AVPacket* m_packet               = nullptr;
    AVFrame* m_frame                 = nullptr;
    int32_t m_videoStreamIndex       = -1;
    AVRational m_timeBase            = {0, 0};
    AVCodecParameters* m_codecParams = nullptr;
    const AVCodec* m_codec           = nullptr;

    bool m_eof     = false;
    bool m_inFrame = false;
};

ImageSequenceStreamRef
ImageSequenceStream::read(const string& filename) noexcept {
    try {
        return std::make_unique<ImageSequenceStreamImpl>(filename);
    } catch (const ImageParseException& e) {
        GeneralLogger::error("Error reading image sequence: " + string(e.what()));
        return nullptr;
    }
}

ImageSequenceStreamImpl::ImageSequenceStreamImpl(const string& filename) {
    try {
        if (avformat_open_input(&m_formatCtx, filename.c_str(), nullptr, nullptr) < 0) {
            throw ImageParseException("Failed to open file: " + filename);
        }
        if (avformat_find_stream_info(m_formatCtx, nullptr) < 0) {
            throw ImageParseException("Failed to find stream info.");
        }
        for (int32_t i = 0; i < m_formatCtx->nb_streams; ++i) {
            if (m_formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                m_videoStreamIndex = i;
                break;
            }
        }
        if (m_videoStreamIndex == -1) {
            throw ImageParseException("No video stream found.");
        }
        m_codecParams = m_formatCtx->streams[m_videoStreamIndex]->codecpar;
        m_codec       = avcodec_find_decoder(m_codecParams->codec_id);
        if (!m_codec) {
            throw ImageParseException("No decoder found.");
        }
        m_codecCtx = avcodec_alloc_context3(m_codec);
        if (!m_codecCtx) {
            throw ImageParseException("Failed to allocate decoder context.");
        }
        avcodec_parameters_to_context(m_codecCtx, m_codecParams);

        if (avcodec_open2(m_codecCtx, m_codec, nullptr) < 0) {
            throw ImageParseException("Failed to open decoder.");
        }
        m_timeBase = m_formatCtx->streams[m_videoStreamIndex]->time_base;
        m_packet   = av_packet_alloc();
        if (!m_packet) {
            throw ImageParseException("Failed to allocate packet.");
        }
        m_frame = av_frame_alloc();
        if (!m_frame) {
            throw ImageParseException("Failed to allocate frame.");
        }
    } catch (const ImageParseException& e) {
        throw;
    } catch (...) {
        throw ImageParseException("Unknown error");
    }
}

ImageSequenceStreamImpl::~ImageSequenceStreamImpl() {
    if (m_inFrame && m_packet) av_packet_unref(m_packet);
    if (m_packet) av_packet_free(&m_packet);
    if (m_frame) av_frame_free(&m_frame);
    if (m_codecCtx) avcodec_free_context(&m_codecCtx);
    if (m_formatCtx) avformat_close_input(&m_formatCtx);
}

FrameRef
ImageSequenceStreamImpl::getNextFrame() noexcept {
    if (m_eof) return nullptr;
    try {
        while (true) {
            if (!m_inFrame) {
                while (true) {
                    if (av_read_frame(m_formatCtx, m_packet) < 0) {
                        throw EOFException();
                    }
                    if (m_packet->stream_index == m_videoStreamIndex) {
                        if (avcodec_send_packet(m_codecCtx, m_packet) >= 0) {
                            break;
                        }
                    }
                    av_packet_unref(m_packet);
                };
                m_inFrame = true;
            }
            try {
                if (avcodec_receive_frame(m_codecCtx, m_frame) < 0) {
                    av_packet_unref(m_packet);
                    m_inFrame = false;
                    continue;
                }
                if (m_frame->width == 0 || m_frame->height == 0) {
                    throw ImageParseException("Invalid dimensions: " +
                                              std::to_string(m_frame->width) + "x" + std::to_string(m_frame->height));
                }

                FrameRef frame = std::make_unique<Frame>();
                frame->width   = m_frame->width;
                frame->height  = m_frame->height;
                frame->buffer.resize(frame->width * frame->height);

                if (m_frame->duration > 0) {
                    uint32_t durationMs = static_cast<uint32_t>(m_frame->duration * av_q2d(m_timeBase) * 1000);
                    frame->delay        = durationMs;
                } else {
                    frame->delay = DEFAULT_DELAY;
                }

                SwsContext* swsCtx = sws_getContext(
                    m_frame->width,
                    m_frame->height,
                    static_cast<AVPixelFormat>(m_frame->format),
                    m_frame->width,
                    m_frame->height,
                    AV_PIX_FMT_BGRA,
                    SWS_POINT,
                    nullptr,
                    nullptr,
                    nullptr);
                if (!swsCtx) {
                    throw ImageParseException("Failed to create sws context.");
                }
                uint8_t* dstData[1]    = {reinterpret_cast<uint8_t*>(frame->buffer.data())};
                int32_t dstLineSize[1] = {static_cast<int>(frame->width * 4)};
                sws_scale(
                    swsCtx,
                    m_frame->data,
                    m_frame->linesize,
                    0,
                    m_frame->height,
                    dstData,
                    dstLineSize);
                sws_freeContext(swsCtx);
                return frame;
            } catch (const ImageParseException& e) {
                GeneralLogger::error("Error processing frame: " + string(e.what()) + ", skipping frame.");
            } catch (...) {
                GeneralLogger::error("Unknown error processing frame, skipping frame.");
            }
        }
    } catch (const EOFException&) {
        m_eof = true;
        return nullptr;
    } catch (const ImageParseException& e) {
        GeneralLogger::error("Error reading image sequence: " + string(e.what()));
        return nullptr;
    } catch (...) {
        GeneralLogger::error("Unknown error reading image sequence.");
        return nullptr;
    }
}

#endif  // IMSQ_USE_FFMPEG