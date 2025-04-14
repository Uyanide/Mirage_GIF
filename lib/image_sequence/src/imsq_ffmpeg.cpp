#ifdef IMSQ_USE_FFMPEG

#include <filesystem>
#include <fstream>
#include <span>
#include <string>

#include "imsq.h"
#include "log.h"
#include "path.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavformat/avformat.h>
#include <libavutil/base64.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

using namespace GIFImage;
using std::string, std::vector, std::span;

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

class ImageSequenceImpl : public ImageSequence {
  public:
    explicit ImageSequenceImpl(const string& filename);

    ~ImageSequenceImpl() noexcept override = default;

    [[nodiscard]] const vector<uint32_t>&
    getDelays() noexcept override {
        return m_delays;
    }

    [[nodiscard]] vector<PixelBGRA>
    getFrameBuffer(uint32_t index,
                   uint32_t width,
                   uint32_t height) noexcept override;

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

  private:
    vector<uint32_t> m_delays;
    uint32_t m_width  = 0;
    uint32_t m_height = 0;
    vector<vector<PixelBGRA>> m_frameBuffer;
};

bool
ImageSequence::initDecoder(const char*) noexcept {
    return true;
}

ImageSequenceRef
ImageSequence::read(const string& filename) noexcept {
    try {
        return std::make_unique<ImageSequenceImpl>(filename);
    } catch (const std::exception& e) {
        GeneralLogger::error("Error reading image sequence: " + string(e.what()));
        return nullptr;
    }
}

ImageSequenceImpl::ImageSequenceImpl(const string& filename) {
    AVFormatContext* formatCtx = nullptr;
    AVCodecContext* codecCtx   = nullptr;
    AVPacket* packet           = nullptr;
    AVFrame* frame             = nullptr;

    const auto defer = [&]() {
        if (frame) av_frame_free(&frame);
        if (packet) av_packet_free(&packet);
        if (codecCtx) avcodec_free_context(&codecCtx);
        if (formatCtx) avformat_close_input(&formatCtx);
    };

    try {
        // open file
        if (avformat_open_input(&formatCtx, filename.c_str(), nullptr, nullptr) < 0) {
            throw ImageParseException("Failed to open file: " + filename);
        }
        // find stream
        if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
            throw ImageParseException("Failed to find stream info.");
        }
        int32_t videoStreamIndex = -1;
        for (uint32_t i = 0; i < formatCtx->nb_streams; i++) {
            if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex = static_cast<int32_t>(i);
                break;
            }
        }
        if (videoStreamIndex == -1) {
            throw ImageParseException("No stream found.");
        }
        // get decoder
        AVCodecParameters* codecParams = formatCtx->streams[videoStreamIndex]->codecpar;
        const AVCodec* codec           = avcodec_find_decoder(codecParams->codec_id);
        if (!codec) {
            throw ImageParseException("No decoder found.");
        }
        codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            throw ImageParseException("Failed to allocate decoder context.");
        }
        avcodec_parameters_to_context(codecCtx, codecParams);

        if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
            throw ImageParseException("Failed to open decoder.");
        }

        packet = av_packet_alloc();
        frame  = av_frame_alloc();

        AVRational timeBase = formatCtx->streams[videoStreamIndex]->time_base;
        while (av_read_frame(formatCtx, packet) >= 0) {
            if (packet->stream_index == videoStreamIndex) {
                if (avcodec_send_packet(codecCtx, packet) < 0) {
                    throw ImageParseException("Failed to send packet.");
                }
                while (avcodec_receive_frame(codecCtx, frame) >= 0) {
                    // size of first frame
                    if (m_width == 0 || m_height == 0) {
                        m_width  = frame->width;
                        m_height = frame->height;
                        if (m_width == 0 || m_height == 0) {
                            throw ImageParseException("Invalid frame size.");
                        }
                    }

                    // duration in ms
                    if (frame->duration > 0) {
                        uint32_t durationMs = static_cast<uint32_t>(frame->duration * av_q2d(timeBase) * 1000);
                        m_delays.push_back(durationMs);
                    } else {
                        m_delays.push_back(DEFAULT_DELAY);
                    }

                    // Convert frame to BGRA8888
                    SwsContext* swsCtx = sws_getContext(
                        frame->width,
                        frame->height,
                        static_cast<AVPixelFormat>(frame->format),
                        m_width,
                        m_height,
                        AV_PIX_FMT_BGRA,
                        SWS_BICUBIC,
                        nullptr,
                        nullptr,
                        nullptr);
                    if (!swsCtx) {
                        throw ImageParseException("Failed to create scaling context.");
                    }
                    vector<PixelBGRA> frameBuffer(m_width * m_height);
                    uint8_t* dstData[1]    = {reinterpret_cast<uint8_t*>(frameBuffer.data())};
                    int32_t dstLineSize[1] = {static_cast<int>(m_width * 4)};
                    sws_scale(
                        swsCtx,
                        frame->data,
                        frame->linesize,
                        0,
                        frame->height,
                        dstData,
                        dstLineSize);
                    sws_freeContext(swsCtx);
                    // resize if size mismatch
                    if (m_width != frame->width || m_height != frame->height) {
                        GeneralLogger::warning("Frame size not consistant, resizing to fit.");
                        // already resized
                    }
                    m_frameBuffer.push_back(std::move(frameBuffer));
                }
            }
            av_packet_unref(packet);
        }
        defer();
    } catch (const std::exception& e) {
        defer();
        throw;
    } catch (...) {
        defer();
        throw ImageParseException("Unknown error");
    }
}

vector<PixelBGRA>
ImageSequence::resizeCover(const vector<PixelBGRA>& buffer,
                           uint32_t origWidth,
                           uint32_t origHeight,
                           uint32_t targetWidth,
                           uint32_t targetHeight) noexcept {
    if (buffer.empty() || origWidth == 0 || origHeight == 0 || targetWidth == 0 || targetHeight == 0) {
        GeneralLogger::error("Invalid buffer or dimensions for resizing.");
        return {};
    }
    if (origWidth == targetWidth && origHeight == targetHeight) {
        return vector(buffer);  // No resizing needed
    }

    double scaleX = static_cast<double>(targetWidth) / origWidth;
    double scaleY = static_cast<double>(targetHeight) / origHeight;
    double scale  = scaleX < scaleY ? scaleY : scaleX;

    uint32_t scaledWidth  = static_cast<uint32_t>(origWidth * scale);
    uint32_t scaledHeight = static_cast<uint32_t>(origHeight * scale);

    uint32_t cropX = (scaledWidth > targetWidth) ? (scaledWidth - targetWidth) / 2 : 0;
    uint32_t cropY = (scaledHeight > targetHeight) ? (scaledHeight - targetHeight) / 2 : 0;

    vector<PixelBGRA> output(targetWidth * targetHeight);

    SwsContext* swsCtx = sws_getContext(
        origWidth,
        origHeight,
        AV_PIX_FMT_BGRA,
        scaledWidth,
        scaledHeight,
        AV_PIX_FMT_BGRA,
        SWS_BICUBIC,
        nullptr,
        nullptr,
        nullptr);

    if (!swsCtx) {
        GeneralLogger::error("Failed to create scaling context");
        return {};
    }

    uint8_t* srcData[1] = {reinterpret_cast<uint8_t*>(const_cast<PixelBGRA*>(buffer.data()))};
    int srcLineSize[1]  = {static_cast<int>(origWidth * sizeof(PixelBGRA))};

    vector<PixelBGRA> scaledBuffer(scaledWidth * scaledHeight);
    uint8_t* dstData[1] = {reinterpret_cast<uint8_t*>(scaledBuffer.data())};
    int dstLineSize[1]  = {static_cast<int>(scaledWidth * sizeof(PixelBGRA))};

    sws_scale(swsCtx, srcData, srcLineSize, 0, origHeight, dstData, dstLineSize);

    sws_freeContext(swsCtx);

    for (uint32_t y = 0; y < targetHeight; ++y) {
        for (uint32_t x = 0; x < targetWidth; ++x) {
            output[y * targetWidth + x] =
                scaledBuffer[(y + cropY) * scaledWidth + (x + cropX)];
        }
    }

    return output;
}

vector<PixelBGRA>
ImageSequenceImpl::getFrameBuffer(uint32_t index,
                                  uint32_t width,
                                  uint32_t height) noexcept {
    if (index >= m_frameBuffer.size()) {
        index %= m_frameBuffer.size();
    }
    if (width == 0 || height == 0 || (width == m_width && height == m_height)) {
        return m_frameBuffer[index];
    } else {
        return ImageSequence::resizeCover(
            m_frameBuffer[index],
            m_width,
            m_height,
            width,
            height);
    }
}

bool
ImageSequence::drawText(vector<PixelBGRA>& buffer,
                        const uint32_t width,
                        const uint32_t height,
                        const std::string& text,
                        const PixelBGRA& textBackgroundColor,
                        const PixelBGRA& textForegroundColor,
                        const double textHeightRatio,
                        const double textPadding,
                        const uint32_t x,
                        const uint32_t y,
                        const std::string& fontFamily) noexcept {
    AVFilterGraph* filter_graph = nullptr;
    AVFrame* in_frame           = nullptr;
    AVFrame* out_frame          = nullptr;

    const auto defer = [&]() {
        if (in_frame) av_frame_free(&in_frame);
        if (out_frame) av_frame_free(&out_frame);
        if (filter_graph) avfilter_graph_free(&filter_graph);
    };

    try {
        if (buffer.empty() || width == 0 || height == 0 || textHeightRatio <= 0.0 || textHeightRatio > 1.0) {
            throw ImageParseException("Invalid buffer or dimensions for drawing text.");
        }
        if (x >= width || y >= height) {
            throw ImageParseException("Invalid x or y coordinates for drawing text.");
        }
        if (buffer.size() != width * height) {
            throw ImageParseException("Buffer size does not match dimensions: " + std::to_string(buffer.size()) +
                                      " != " + std::to_string(width * height));
        }

        filter_graph = avfilter_graph_alloc();
        if (!filter_graph) {
            throw ImageParseException("Failed to allocate filter graph");
        }

        const AVFilter* buffersrc  = avfilter_get_by_name("buffer");
        const AVFilter* buffersink = avfilter_get_by_name("buffersink");
        const AVFilter* drawtext   = avfilter_get_by_name("drawtext");
        const AVFilter* format     = avfilter_get_by_name("format");

        if (!buffersrc || !buffersink || !drawtext || !format) {
            throw ImageParseException("Failed to get required filters");
        }

        AVFilterContext* buffersrc_ctx  = nullptr;
        AVFilterContext* buffersink_ctx = nullptr;
        AVFilterContext* drawtext_ctx   = nullptr;
        AVFilterContext* format_ctx     = nullptr;

        uint32_t textBoxHeight = static_cast<uint32_t>(height * textHeightRatio);
        uint32_t fontSize      = static_cast<uint32_t>(textBoxHeight);

        char args[512];
        snprintf(args, sizeof(args), "video_size=%dx%d:pix_fmt=%d:time_base=1/1:pixel_aspect=1/1", width, height, AV_PIX_FMT_BGRA);

        int ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, nullptr, filter_graph);
        if (ret < 0) {
            throw ImageParseException("Cannot create buffer source");
        }

        ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", nullptr, nullptr, filter_graph);
        if (ret < 0) {
            throw ImageParseException("Cannot create buffer sink");
        }

        enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_BGRA, AV_PIX_FMT_NONE};

        ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
        if (ret < 0) {
            throw ImageParseException("Cannot set output pixel format");
        }

        ret = avfilter_graph_create_filter(&format_ctx, format, "format", "pix_fmts=bgra", nullptr, filter_graph);
        if (ret < 0) {
            throw ImageParseException("Cannot create format filter");
        }

        char drawtext_args[1024];
        snprintf(drawtext_args,
                 sizeof(drawtext_args),
                 "fontfile=%s:text=%s:fontsize=%d:fontcolor=0x%02x%02x%02x@0x%02x:box=1:boxcolor=0x%02x%02x%02x@0x%02x:x=%d:y=%d",
                 fontFamily.c_str(),
                 text.c_str(),
                 fontSize,
                 textForegroundColor.r,
                 textForegroundColor.g,
                 textForegroundColor.b,
                 textForegroundColor.a,
                 textBackgroundColor.r,
                 textBackgroundColor.g,
                 textBackgroundColor.b,
                 textBackgroundColor.a,
                 x,
                 y);

        ret = avfilter_graph_create_filter(&drawtext_ctx, drawtext, "drawtext", drawtext_args, nullptr, filter_graph);
        if (ret < 0) {
            throw ImageParseException("Cannot create drawtext filter");
        }

        ret = avfilter_link(buffersrc_ctx, 0, drawtext_ctx, 0);
        if (ret < 0) {
            throw ImageParseException("Cannot link buffer source to drawtext");
        }

        ret = avfilter_link(drawtext_ctx, 0, format_ctx, 0);
        if (ret < 0) {
            throw ImageParseException("Cannot link drawtext to format");
        }

        ret = avfilter_link(format_ctx, 0, buffersink_ctx, 0);
        if (ret < 0) {
            throw ImageParseException("Cannot link format to buffer sink");
        }

        ret = avfilter_graph_config(filter_graph, nullptr);
        if (ret < 0) {
            throw ImageParseException("Cannot configure filter graph");
        }

        AVFrame* in_frame  = av_frame_alloc();
        AVFrame* out_frame = av_frame_alloc();
        if (!in_frame || !out_frame) {
            throw ImageParseException("Failed to allocate frames");
        }

        in_frame->width  = width;
        in_frame->height = height;
        in_frame->format = AV_PIX_FMT_BGRA;

        ret = av_frame_get_buffer(in_frame, 0);
        if (ret < 0) {
            throw ImageParseException("Failed to allocate frame buffer");
        }

        ret = av_frame_make_writable(in_frame);
        if (ret < 0) {
            throw ImageParseException("Failed to make frame writable");
        }

        for (uint32_t y = 0; y < height; y++) {
            memcpy(in_frame->data[0] + y * in_frame->linesize[0],
                   buffer.data() + y * width,
                   width * sizeof(PixelBGRA));
        }

        ret = av_buffersrc_add_frame_flags(buffersrc_ctx, in_frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0) {
            throw ImageParseException("Error while feeding the filtergraph");
        }

        ret = av_buffersink_get_frame(buffersink_ctx, out_frame);
        if (ret < 0) {
            throw ImageParseException("Error while getting frame from the filtergraph");
        }

        for (uint32_t y = 0; y < height; y++) {
            memcpy(buffer.data() + y * width,
                   out_frame->data[0] + y * out_frame->linesize[0],
                   width * sizeof(PixelBGRA));
        }

        defer();
        return true;
    } catch (const std::exception& e) {
        GeneralLogger::error("Error drawing text: " + string(e.what()));
    } catch (...) {
        GeneralLogger::error("Unknown error drawing text.");
    }
    defer();
    return false;
}

vector<PixelBGRA>
ImageSequence::parseBase64(const string& base64) noexcept {
    vector<PixelBGRA> result;

    AVFormatContext* formatCtx = nullptr;
    AVIOContext* pbCtx         = nullptr;
    AVCodecContext* codecCtx   = nullptr;
    AVPacket* packet           = nullptr;
    AVFrame* frame             = nullptr;
    uint8_t* buffer            = nullptr;

    const auto defer = [&]() {
        if (frame) av_frame_free(&frame);
        if (packet) av_packet_free(&packet);
        if (codecCtx) avcodec_free_context(&codecCtx);
        if (pbCtx) {
            av_freep(&(pbCtx->buffer));
            avio_context_free(&pbCtx);
        } else if (buffer) {
            av_freep(&buffer);
        }
        if (formatCtx) avformat_close_input(&formatCtx);
    };

    try {
        const auto pos = base64.find("base64,");
        if (pos == string::npos) {
            throw ImageParseException("Invalid base64 data.");
        }
        const auto substr      = std::string_view(base64.data() + pos + 7);
        const auto decodedSize = AV_BASE64_DECODE_SIZE(substr.size());
        vector<uint8_t> decodedData(decodedSize);

        int actualSize = av_base64_decode(decodedData.data(), substr.data(), decodedSize);
        if (actualSize <= 0) {
            throw ImageParseException("Failed to decode base64 data");
        }
        decodedData.resize(actualSize);

        AVIOContext* ioCtx = nullptr;
        if (avio_open_dyn_buf(&ioCtx) < 0) {
            throw ImageParseException("Failed to create memory IO context");
        }

        avio_write(ioCtx, decodedData.data(), actualSize);

        formatCtx = avformat_alloc_context();
        if (!formatCtx) {
            uint8_t* buffer = nullptr;
            avio_close_dyn_buf(ioCtx, &buffer);
            av_free(buffer);
            throw ImageParseException("Failed to allocate format context");
        }

        int bufferSize = avio_close_dyn_buf(ioCtx, &buffer);

        pbCtx = avio_alloc_context(
            buffer,
            bufferSize,
            0,
            nullptr,
            nullptr,
            nullptr,
            nullptr);
        if (!pbCtx) {
            throw ImageParseException("Failed to create AVIO context");
        }

        formatCtx->pb = pbCtx;

        if (avformat_open_input(&formatCtx, nullptr, nullptr, nullptr) < 0) {
            throw ImageParseException("Failed to open memory input");
        }

        if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
            throw ImageParseException("Failed to find stream info");
        }

        int videoStreamIdx = -1;
        for (unsigned i = 0; i < formatCtx->nb_streams; i++) {
            if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIdx = i;
                break;
            }
        }
        if (videoStreamIdx == -1) {
            throw ImageParseException("No video stream found in data");
        }

        AVCodecParameters* codecParams = formatCtx->streams[videoStreamIdx]->codecpar;
        const AVCodec* codec           = avcodec_find_decoder(codecParams->codec_id);
        if (!codec) {
            throw ImageParseException("Codec not found");
        }

        codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            throw ImageParseException("Failed to allocate codec context");
        }

        if (avcodec_parameters_to_context(codecCtx, codecParams) < 0) {
            throw ImageParseException("Failed to copy codec parameters");
        }

        if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
            throw ImageParseException("Failed to open codec");
        }

        packet = av_packet_alloc();
        frame  = av_frame_alloc();

        uint32_t width  = 0;
        uint32_t height = 0;

        while (av_read_frame(formatCtx, packet) >= 0) {
            if (packet->stream_index == videoStreamIdx) {
                if (avcodec_send_packet(codecCtx, packet) < 0) {
                    throw ImageParseException("Failed to send packet to decoder");
                }

                while (avcodec_receive_frame(codecCtx, frame) >= 0) {
                    width  = frame->width;
                    height = frame->height;

                    SwsContext* swsCtx = sws_getContext(
                        width,
                        height,
                        static_cast<AVPixelFormat>(frame->format),
                        width,
                        height,
                        AV_PIX_FMT_BGRA,
                        SWS_BICUBIC,
                        nullptr,
                        nullptr,
                        nullptr);

                    if (!swsCtx) {
                        throw ImageParseException("Failed to create scaling context");
                    }

                    result.resize(width * height);
                    uint8_t* dstData[1]    = {reinterpret_cast<uint8_t*>(result.data())};
                    int32_t dstLinesize[1] = {static_cast<int>(width * sizeof(PixelBGRA))};

                    sws_scale(
                        swsCtx,
                        frame->data,
                        frame->linesize,
                        0,
                        frame->height,
                        dstData,
                        dstLinesize);

                    sws_freeContext(swsCtx);
                    break;
                }

                if (!result.empty()) {
                    break;
                }
            }
            av_packet_unref(packet);
        }
        if (result.empty()) {
            throw ImageParseException("No frames found in base64 data");
        }
    } catch (const std::exception& e) {
        GeneralLogger::error("Error parsing base64 image: " + string(e.what()));
    } catch (...) {
        GeneralLogger::error("Unknown error parsing base64 image");
    }
    defer();
    return result;
}

bool
ImageSequence::drawMark(vector<PixelBGRA>& buffer,
                        const uint32_t width,
                        const uint32_t height,
                        const vector<PixelBGRA> markBuffer,
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

#endif  // IMSQ_USE_FFMPEG