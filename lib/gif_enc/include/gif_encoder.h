#ifndef GIF_ENCODER_H
#define GIF_ENCODER_H

#include <functional>
#include <span>
#include <string>
#include <vector>

#include "def.h"

namespace GIFEnc {
class GIFEncoder {
  public:
    using WriteChunkCallback = std::function<bool(const std::span<const uint8_t>&)>;

    GIFEncoder(const WriteChunkCallback& writeChunkCallback,
               uint32_t width,
               uint32_t height,
               uint32_t backgroundIndex,
               uint32_t minCodeLength,
               bool hasTransparency,
               uint32_t transparentIndex,
               uint32_t loops,
               bool hasGlobalColorTable,
               const std::vector<PixelBGRA>& globalColorTable = {});

    ~GIFEncoder();

    /**
     * @brief Add a frame to the GIF file.
     * @param frame         The frame data as indexes in palette.
     * @param delay         Frame duration in milliseconds.
     * @param disposalMethod 0-3
     * @param minCodeLength If 0, the global one will be used.
     * @param palette       The color palette for the frame.
     *                      If empty, the global color table will be used.
     */
    void
    addFrame(const std::span<const uint8_t>& frame,
             uint32_t delay,
             uint32_t disposalMethod,
             uint32_t minCodeLength                = 0,
             const std::vector<PixelBGRA>& palette = {});

    /**
     * @brief Add a frame to the GIF file.
     * @param frame         The frame data that can be directly written to the
     *                      GIF file.
     * @param delay         Frame duration in milliseconds.
     * @param disposalMethod 0-3
     * @param minCodeLength If 0, the global one will be used.
     * @param palette       The color palette for the frame.
     *                      If empty, the global color table will be used.
     */
    void
    addFrameCompressed(const std::span<const uint8_t>& frame,
                       uint32_t delay,
                       uint32_t disposalMethod,
                       uint32_t minCodeLength                = 0,
                       const std::vector<PixelBGRA>& palette = {});

    /**
     * @brief Add an application extension chunk to the GIF file.
     * @param identifier    The 8 byte identifier of the application.
     * @param authentication The 3 byte authentication code.
     * @param data Unchunked data
     */
    void
    addApplicationExtension(const std::string& identifier,
                            const std::string& authentication,
                            const std::span<const uint8_t>& data);

    bool
    finish();

    void
    deleteFile();

  private:
    void
    writeFile(const std::span<const uint8_t>& data);

    void
    writeFile(uint8_t byte);

  private:
    WriteChunkCallback m_writeChunkCallback;
    uint32_t m_width            = 0;
    uint32_t m_height           = 0;
    uint32_t m_minCodeLength    = 0;
    bool m_hasTransparency      = false;
    uint32_t m_transparentIndex = 0;
    std::vector<PixelBGRA> m_globalColorTable;

    bool m_finished = false;
};
};  // namespace GIFEnc

#endif  // GIF_ENCODER_H