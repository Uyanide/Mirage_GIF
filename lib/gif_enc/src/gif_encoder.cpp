#include "gif_encoder.h"

#include <iostream>
#include <string>
#include <vector>

#include "def.h"
#include "gif_exception.h"
#include "gif_format.h"
#include "gif_lzw.h"
using std::vector, std::span, std::string;

static inline bool
checkCodeLengthValid(u32 minCodeLength, size_t paletteSize) {
    if (minCodeLength < 2 || minCodeLength > 8) {
        return false;
    }
    if (paletteSize > (1ull << minCodeLength) || paletteSize <= (1ull << (minCodeLength - 1))) {
        return false;
    }
    return true;
}

static inline bool
checkIndexesValid(const std::span<u8>& codes, u32 minCodeLength, size_t paletteSize) {
    const auto maxIndex = paletteSize - 1;
    for (const auto& code : codes) {
        if (code > maxIndex) {
            return false;
        }
    }
    return true;
}

GIFEnc::GIFEncoder::GIFEncoder(const string& outPath,
                               const u32 width,
                               const u32 height,
                               const u32 backgroundIndex,
                               const u32 minCodeLength,
                               const bool hasTransparency,
                               const u32 transparentIndex,
                               const u32 loops,
                               const bool hasGlobalColorTable,
                               const vector<PixelBGRA>& globalColorTable)
    : m_width(width),
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
            throw GIFEnc::GIFEncodeException("Color table size mismatch: " + std::to_string(globalColorTable.size()));
        }
    }
    if (hasTransparency && (transparentIndex >= globalColorTable.size())) {
        throw GIFEnc::GIFEncodeException("Transparent index out of range");
    }
    m_outPath = std::filesystem::path(localizePath(outPath)).replace_extension(".gif");
    m_file.open(m_outPath, std::ios::binary);
    if (!m_file || !m_file.is_open()) {
        m_finished = true;
        throw GIFEnc::GIFEncodeException("Failed to open output file");
    }
    m_file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    auto header = GIFEnc::gifHeader(
        m_width, m_height, backgroundIndex, m_minCodeLength, loops, hasGlobalColorTable, m_globalColorTable);
    if (header.empty()) {
        m_finished = true;
        m_file.close();
        deleteFile();
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
GIFEnc::GIFEncoder::addFrame(const span<u8>& frame,
                             u32 delay,
                             u32 minCodeLength,
                             const std::vector<PixelBGRA>& palette) {
    if (m_finished) {
        return;
    }

    if (m_globalColorTable.empty() && (minCodeLength == 0 || palette.empty())) {
        throw GIFEnc::GIFEncodeException("Local palette should be provided when global color table is empty");
    }

    u32 mcl;
    const std::vector<PixelBGRA>* pal = nullptr;

    if (minCodeLength == 0) {
        mcl = m_minCodeLength;
    } else {
        mcl = minCodeLength;
        if (!palette.empty()) {
            if (!checkCodeLengthValid(mcl, palette.size())) {
                throw GIFEnc::GIFEncodeException("Color table size mismatch");
            }
            if (!checkIndexesValid(frame, mcl, palette.size())) {
                throw GIFEnc::GIFEncodeException("Color index out of range");
            }
            pal = &palette;
        } else if (mcl != m_minCodeLength) {
            throw GIFEnc::GIFEncodeException("Invalid min code size");
        }
    }

    auto buffer = GIFEnc::gifFrameHeader(
        m_width, m_height, delay, m_hasTransparency, m_transparentIndex, mcl, pal ? *pal : vector<PixelBGRA>{});

    if (buffer.empty()) {
        throw GIFEnc::GIFEncodeException("Frame header generation failed");
    }

    bool isFirst          = true;
    const auto compressed = GIFEnc::LZW::compressStream(
        [&frame, &isFirst]() -> span<u8> {
            if (isFirst) {
                isFirst = false;
                return {frame.data(), frame.size()};
            } else {
                return {};
            }
        },
        [&buffer](const span<u8>& data) {
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
GIFEnc::GIFEncoder::addFrameCompressed(const span<u8>& frame,
                                       u32 delay,
                                       u32 minCodeLength,
                                       const std::vector<PixelBGRA>& palette) {
    if (m_finished) {
        return;
    }

    if (m_globalColorTable.empty() && (minCodeLength == 0 || palette.empty())) {
        throw GIFEnc::GIFEncodeException("Local palette should be provided when global color table is empty");
    }

    u32 mcl;
    const std::vector<PixelBGRA>* pal = nullptr;

    if (minCodeLength == 0) {
        mcl = m_minCodeLength;
    } else {
        mcl = minCodeLength;
        if (!palette.empty()) {
            if (!checkCodeLengthValid(mcl, palette.size())) {
                throw GIFEnc::GIFEncodeException("Color table size mismatch");
            }
            if (!checkIndexesValid(frame, mcl, palette.size())) {
                throw GIFEnc::GIFEncodeException("Color index out of range");
            }
            pal = &palette;
        } else if (mcl != m_minCodeLength) {
            throw GIFEnc::GIFEncodeException("Invalid min code size");
        }
    }

    auto buffer = GIFEnc::gifFrameHeader(
        m_width, m_height, delay, m_hasTransparency, m_transparentIndex, mcl, pal ? *pal : vector<PixelBGRA>{});
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
                                            const vector<u8>& data) {
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
    m_file.close();
    m_finished = true;
    return true;
}

void
GIFEnc::GIFEncoder::writeFile(const span<u8>& data) {
    if (m_finished) return;
    try {
        m_file.write(reinterpret_cast<const char*>(data.data()), data.size());
    } catch (const std::ios_base::failure&) {
        m_finished = true;
        m_file.close();
        deleteFile();
        throw GIFEnc::GIFEncodeException("Failed to write");
    }
}

void
GIFEnc::GIFEncoder::writeFile(u8 byte) {
    if (m_finished) return;
    try {
        m_file.write(reinterpret_cast<const char*>(&byte), 1);
    } catch (const std::ios_base::failure&) {
        m_finished = true;
        m_file.close();
        deleteFile();
        throw GIFEnc::GIFEncodeException("Failed to write");
    }
}

void
GIFEnc::GIFEncoder::deleteFile() {
    if (m_file.is_open()) {
        m_file.close();
    }
    if (std::filesystem::exists(m_outPath)) {
        std::filesystem::remove(m_outPath);
    }
}