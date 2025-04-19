#include "gif_encoder.h"

#include <string>
#include <vector>

#include "def.h"
#include "gif_exception.h"
#include "gif_format.h"
#include "gif_lzw.h"
using std::vector, std::span, std::string;

static bool
checkCodeLengthValid(uint32_t minCodeLength, size_t paletteSize) {
    if (minCodeLength < 2 || minCodeLength > 8) {
        return false;
    }
    if (paletteSize > (1ull << minCodeLength) || paletteSize <= (1ull << (minCodeLength - 1))) {
        return false;
    }
    return true;
}

static bool
checkIndexesValid(const std::span<const uint8_t>& codes, size_t paletteSize) {
    const auto maxIndex = paletteSize - 1;
    for (const auto& code : codes) {
        if (code > maxIndex) {
            return false;
        }
    }
    return true;
}

GIFEnc::GIFEncoder::GIFEncoder(const WriteChunkCallback& writeChunkCallback,
                               const uint32_t width,
                               const uint32_t height,
                               const uint32_t backgroundIndex,
                               const uint32_t minCodeLength,
                               const bool hasTransparency,
                               const uint32_t transparentIndex,
                               const uint32_t loops,
                               const bool hasGlobalColorTable,
                               const vector<PixelBGRA>& globalColorTable)
    : m_writeChunkCallback(writeChunkCallback),
      m_width(width),
      m_height(height),
      m_minCodeLength(minCodeLength),
      m_hasTransparency(hasTransparency),
      m_transparentIndex(transparentIndex),
      m_globalColorTable(hasGlobalColorTable ? globalColorTable : vector<PixelBGRA>{}) {
    if (minCodeLength < 2 || minCodeLength > 8) {
        throw GIFEnc::GIFEncodeException("Invalid min code size");
    }
    if (hasGlobalColorTable) {
        if (!checkCodeLengthValid(minCodeLength, globalColorTable.size())) {
            throw GIFEnc::GIFEncodeException("Color table size mismatch: " +
                                             std::to_string(globalColorTable.size()));
        }
    }
    if (hasTransparency && (transparentIndex >= globalColorTable.size())) {
        throw GIFEnc::GIFEncodeException("Transparent index out of range");
    }
    auto header = GIFEnc::gifHeader(
        m_width,
        m_height,
        backgroundIndex,
        m_minCodeLength,
        loops,
        hasGlobalColorTable,
        m_globalColorTable);
    if (header.empty()) {
        m_finished = true;
        throw GIFEnc::GIFEncodeException("Header generation failed");
    }
    writeFile(header);
}

GIFEnc::GIFEncoder::~GIFEncoder() {
    if (!m_finished) {
        finish();
    }
}

void
GIFEnc::GIFEncoder::addFrame(
    const span<const uint8_t>& frame,
    uint32_t delay,
    uint32_t disposalMethod,
    uint32_t minCodeLength,
    const std::vector<PixelBGRA>& palette) {
    if (m_finished) {
        return;
    }

    if (m_globalColorTable.empty() && (minCodeLength == 0 || palette.empty())) {
        throw GIFEnc::GIFEncodeException(
            "Local palette should be provided when global color table is "
            "empty");
    }
    if (frame.size() != m_width * m_height) {
        throw GIFEnc::GIFEncodeException("Frame size mismatch");
    }

    uint32_t mcl;
    const std::vector<PixelBGRA>* pal = nullptr;

    if (minCodeLength == 0) {
        mcl = m_minCodeLength;
    } else {
        mcl = minCodeLength;
        if (!palette.empty()) {
            if (!checkCodeLengthValid(mcl, palette.size())) {
                throw GIFEnc::GIFEncodeException("Color table size mismatch");
            }
            if (!checkIndexesValid(frame, palette.size())) {
                throw GIFEnc::GIFEncodeException("Color index out of range");
            }
            pal = &palette;
        } else if (mcl != m_minCodeLength) {
            throw GIFEnc::GIFEncodeException("Invalid min code size");
        }
    }

    auto buffer = GIFEnc::gifFrameHeader(m_width,
                                         m_height,
                                         delay,
                                         m_hasTransparency,
                                         m_transparentIndex,
                                         disposalMethod,
                                         mcl,
                                         pal ? *pal : vector<PixelBGRA>{});

    if (buffer.empty()) {
        throw GIFEnc::GIFEncodeException("Frame header generation failed");
    }

    bool isFirst          = true;
    const auto compressed = GIFEnc::LZW::compressStream(
        [&frame, &isFirst]() -> span<const uint8_t> {
            if (isFirst) {
                isFirst = false;
                return {frame.data(), frame.size()};
            } else {
                return {};
            }
        },
        [&buffer](const span<const uint8_t>& data) {
            if (data.empty()) return;
            buffer.push_back(data.size());
            buffer.insert(buffer.end(), data.begin(), data.end());
        },
        nullptr,
        mcl,
        255);
    if (compressed == 0) {
        throw GIFEnc::GIFEncodeException("Compression failed");
    }

    buffer.push_back(0);
    writeFile(buffer);
}

void
GIFEnc::GIFEncoder::addFrameCompressed(
    const span<const uint8_t>& frame,
    uint32_t delay,
    uint32_t disposalMethod,
    uint32_t minCodeLength,
    const std::vector<PixelBGRA>& palette) {
    if (m_finished) {
        return;
    }

    if (m_globalColorTable.empty() && (minCodeLength == 0 || palette.empty())) {
        throw GIFEnc::GIFEncodeException(
            "Local palette should be provided when global color table is "
            "empty");
    }

    uint32_t mcl;
    const std::vector<PixelBGRA>* pal = nullptr;

    if (minCodeLength == 0) {
        mcl = m_minCodeLength;
    } else {
        mcl = minCodeLength;
        if (!palette.empty()) {
            if (!checkCodeLengthValid(mcl, palette.size())) {
                throw GIFEnc::GIFEncodeException("Color table size mismatch");
            }
            if (!checkIndexesValid(frame, palette.size())) {
                throw GIFEnc::GIFEncodeException("Color index out of range");
            }
            pal = &palette;
        } else if (mcl != m_minCodeLength) {
            throw GIFEnc::GIFEncodeException("Invalid min code size");
        }
    }

    vector buffer = GIFEnc::gifFrameHeader(m_width,
                                           m_height,
                                           delay,
                                           m_hasTransparency,
                                           m_transparentIndex,
                                           disposalMethod,
                                           mcl,
                                           pal ? *pal : vector<PixelBGRA>{});
    if (buffer.empty()) {
        throw GIFEnc::GIFEncodeException("Frame header generation failed");
    }

    if (frame.empty()) {
        buffer.push_back(0);
    } else {
        buffer.insert(buffer.end(), frame.begin(), frame.end());
    }
    writeFile(buffer);
}

void
GIFEnc::GIFEncoder::addApplicationExtension(const string& identifier,
                                            const string& authentication,
                                            const span<const uint8_t>& data) {
    if (m_finished) {
        return;
    }
    auto ext = GIFEnc::gifApplicationExtension(identifier, authentication, data);
    if (ext.empty()) {
        throw GIFEnc::GIFEncodeException("Extension generation failed");
    }
    writeFile(ext);
}

bool
GIFEnc::GIFEncoder::finish() {
    if (m_finished) {
        return false;
    }
    writeFile(GIFEnc::GIF_END);
    m_finished = true;
    return true;
}

void
GIFEnc::GIFEncoder::writeFile(const span<const uint8_t>& data) {
    if (m_finished) return;
    if (!m_writeChunkCallback(data)) {
        m_finished = true;
        throw GIFEnc::GIFEncodeException("Failed to write");
    }
}

void
GIFEnc::GIFEncoder::writeFile(const uint8_t byte) {
    if (m_finished) return;
    if (!m_writeChunkCallback({&byte, 1})) {
        m_finished = true;
        throw GIFEnc::GIFEncodeException("Failed to write");
    }
}