#ifndef GIF_ENCODER_H
#define GIF_ENCODER_H

#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

#include "def.h"
#include "path.h"

namespace GIFEnc {
class GIFEncoder {
  public:
    GIFEncoder() { m_finished = true; }

    GIFEncoder(const std::string& outPath,
               u32 width,
               u32 height,
               u32 backgroundIndex,
               u32 minCodeLength,
               bool hasTransparency,
               u32 transparentIndex,
               u32 loops,
               bool hasGlobalColorTable,
               const std::vector<PixelBGRA>& globalColorTable = {});

    ~GIFEncoder();

    /**
     * @brief Add a frame to the GIF file.
     * @param frame         The frame data as indexes in palette.
     * @param delay         Frame duration in milliseconds.
     * @param minCodeLength If 0, the global one will be used.
     * @param palette       The color palette for the frame.
     *                      If empty, the global color table will be used.
     */
    void
    addFrame(const std::span<u8>& frame,
             u32 delay,
             u32 disposalMethod,
             u32 minCodeLength                     = 0,
             const std::vector<PixelBGRA>& palette = {});

    /**
     * @brief Add a frame to the GIF file.
     * @param frame         The frame data that can be directly written to the
     *                      GIF file.
     * @param delay         Frame duration in milliseconds.
     * @param minCodeLength If 0, the global one will be used.
     * @param palette       The color palette for the frame.
     *                      If empty, the global color table will be used.
     */
    void
    addFrameCompressed(const std::span<u8>& frame,
                       u32 delay,
                       u32 disposalMethod,
                       u32 minCodeLength                     = 0,
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
                            const std::vector<u8>& data);

    bool
    finish();

    void
    deleteFile();

    inline std::string
    getFileName() const {
        return deLocalizePath(m_outPath);
    }

  private:
    void
    writeFile(const std::span<u8>& data);

    void
    writeFile(u8 byte);

  private:
    std::ofstream m_file;
    std::filesystem::path m_outPath;
    u32 m_width            = 0;
    u32 m_height           = 0;
    u32 m_minCodeLength    = 0;
    bool m_hasTransparency = false;
    u32 m_transparentIndex = 0;
    std::vector<PixelBGRA> m_globalColorTable;

    bool m_finished = false;
};
};  // namespace GIFEnc

#endif  // GIF_ENCODER_H