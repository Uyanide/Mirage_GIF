#include <utility>
#include <vector>

#include "gif_lzw.h"
using std::vector, std::span;

class LZWDecompressImpl {
    static constexpr uint16_t NONE_CODE = 0xFFFFu;

    struct LZWNode {                // data: prefix + final byte
        uint32_t len  = 0;          // length of data
        uint16_t prev = NONE_CODE;  // index of prefix
        uint8_t data  = 0;          // final byte
    };

  public:
    LZWDecompressImpl(const GIFEnc::LZW::WriteCallback& write,
                      const GIFEnc::LZW::ErrorCallback& onError,
                      uint32_t minCodeSize,
                      size_t writeChunkSize);
    ~LZWDecompressImpl();

    void
    process(const span<const uint8_t>& data);
    size_t
    finish();

    [[nodiscard]] bool
    finished() const {
        return m_finished;
    }

  private:
    void
    _reset();
    uint16_t
    _insertDict(uint16_t prev, uint8_t data);
    uint8_t
    _writeCode(uint16_t code);
    void
    _appendResult(uint8_t data);
    void
    _flushResult(size_t maxSize);
    void
    _onError();

    vector<uint8_t> m_result;
    size_t m_resultTotalSize = 0;
    const size_t m_writeChunkSize;
    const GIFEnc::LZW::WriteCallback& m_write;
    const GIFEnc::LZW::ErrorCallback& m_onError;

    uint32_t m_buffer = 0, m_bufferSize = 0;

    vector<LZWNode> m_dict;
    uint32_t m_dictSize = 0;

    const uint32_t m_minCodeSize;
    uint32_t m_currCodeSize = 0;
    const uint16_t m_cleanCode, m_endCode;

    uint16_t m_prevCode = NONE_CODE;

    bool m_finished = false;
};

LZWDecompressImpl::LZWDecompressImpl(const GIFEnc::LZW::WriteCallback& write,
                                     const GIFEnc::LZW::ErrorCallback& onError,
                                     const uint32_t minCodeSize,
                                     const size_t writeChunkSize)
    : m_writeChunkSize(writeChunkSize),
      m_write(std::move(write)),
      m_onError(std::move(onError)),
      m_minCodeSize(minCodeSize),
      m_cleanCode(1 << minCodeSize),
      m_endCode(m_cleanCode + 1) {
    m_dict.resize(GIFEnc::LZW::MAX_DICT_SIZE);

    for (uint16_t i = 0; i < m_cleanCode; i++) {
        m_dict[i].data = static_cast<uint8_t>(i);
        m_dict[i].len  = 1;
    }
    _reset();
}

LZWDecompressImpl::~LZWDecompressImpl() = default;

void
LZWDecompressImpl::_reset() {
    m_currCodeSize = m_minCodeSize + 1;
    m_dictSize     = m_endCode + 1;
    m_prevCode     = NONE_CODE;
    if (m_dictSize >= 1u << m_currCodeSize) {
        m_currCodeSize++;
    }
}

void
LZWDecompressImpl::process(const span<const uint8_t>& data) {
    if (m_finished) {
        return;
    }
    size_t pos   = 0;
    auto popCode = [this, &data, &pos]() -> int32_t {
        while (m_bufferSize < m_currCodeSize) {
            if (pos >= data.size()) {
                return -1;
            }
            m_buffer |= data[pos++] << m_bufferSize;
            m_bufferSize += 8;
        }
        const uint16_t code = m_buffer & ((1u << m_currCodeSize) - 1u);
        m_buffer >>= m_currCodeSize;
        m_bufferSize -= m_currCodeSize;
        return code;
    };
    int32_t code = popCode();
    if (m_result.empty() && code == static_cast<int32_t>(m_cleanCode)) {
        code = popCode();
    }
    while (code != -1 && !m_finished) {
        if (const auto codeU = static_cast<uint16_t>(code); codeU == m_cleanCode) {
            _reset();
        } else if (codeU == m_endCode) {
            m_finished = true;
            return;
        } else {
            if (codeU >= m_dictSize) {
                if (m_prevCode == NONE_CODE || m_prevCode >= m_dictSize) {
                    _onError();
                    return;
                }
                uint8_t newData = _writeCode(m_prevCode);
                _appendResult(newData);
                m_prevCode = _insertDict(m_prevCode, newData);
            } else {
                uint8_t newData = _writeCode(codeU);
                if (m_prevCode != NONE_CODE) {
                    if (m_prevCode >= m_dictSize) {
                        _onError();
                        return;
                    }
                    m_prevCode = _insertDict(m_prevCode, newData);
                }
                m_prevCode = codeU;
            }
        }
        code = popCode();
    }
}

size_t
LZWDecompressImpl::finish() {
    if (!m_finished) {
        _onError();
        return 0;
    }
    _flushResult(0);
    return m_resultTotalSize;
}

uint16_t
LZWDecompressImpl::_insertDict(uint16_t prev, uint8_t data) {
    if (m_dictSize >= GIFEnc::LZW::MAX_DICT_SIZE) {
        _onError();
        return NONE_CODE;
    }
    m_dict[m_dictSize].prev = prev;
    m_dict[m_dictSize].data = data;
    m_dict[m_dictSize].len  = m_dict[prev].len + 1;
    m_dictSize++;
    if (m_currCodeSize < GIFEnc::LZW::MAX_CODE_SIZE && m_dictSize >= 1u << m_currCodeSize) {
        m_currCodeSize++;
    }
    return m_dictSize - 1;
}

uint8_t
LZWDecompressImpl::_writeCode(uint16_t code) {
    m_result.resize(m_result.size() + m_dict[code].len);
    uint16_t currCode = code, firstCode = code;
    for (auto it = prev(m_result.end()); currCode != NONE_CODE; --it) {
        firstCode        = currCode;
        const auto& data = m_dict[currCode];
        *it              = data.data;
        currCode         = data.prev;
    }
    if (m_result.size() >= m_writeChunkSize) {
        uint8_t *const l = m_result.data(), *const r = l + m_result.size(), *pos = l;
        for (; r - pos >= 0 && static_cast<size_t>(r - pos) >= m_writeChunkSize; pos += m_writeChunkSize) {
            m_write(span<const uint8_t>(pos, m_writeChunkSize));
            m_resultTotalSize += m_writeChunkSize;
        }
        m_result.erase(m_result.begin(), m_result.begin() + (pos - l));
    }
    return m_dict[firstCode].data;
}

void
LZWDecompressImpl::_appendResult(uint8_t data) {
    m_result.push_back(data);
    _flushResult(m_writeChunkSize);
};

void
LZWDecompressImpl::_flushResult(size_t maxSize) {
    if (m_result.size() >= maxSize) {
        m_write(span<const uint8_t>(m_result.data(), m_result.size()));
        m_resultTotalSize += m_result.size();
        m_result.clear();
    }
}

void
LZWDecompressImpl::_onError() {
    m_finished = true;
    m_result.clear();
    m_resultTotalSize = 0;
    _reset();
    if (m_onError) {
        m_onError();
    }
}

size_t
GIFEnc::LZW::decompressStream(const ReadCallback& read,
                              const GIFEnc::LZW::WriteCallback& write,
                              const GIFEnc::LZW::ErrorCallback& onError,
                              const uint32_t minCodeSize,
                              const size_t writeChunkSize) noexcept {
    if (minCodeSize < 2) {
        return 0;
    }
    if (read == nullptr || write == nullptr) {
        return 0;
    }
    auto decoder = LZWDecompressImpl(write, onError, minCodeSize, writeChunkSize);
    while (!decoder.finished()) {
        auto data = read();
        if (data.empty()) break;
        decoder.process(data);
    }
    return decoder.finish();
}

vector<uint8_t>
GIFEnc::LZW::decompress(const span<const uint8_t>& data, const uint32_t minCodeSize) noexcept {
    if (minCodeSize < 2) {
        return {};
    }
    vector<uint8_t> out;
    auto decoder = LZWDecompressImpl([&out](const span<const uint8_t>& data) { out.insert(out.end(), data.begin(), data.end()); },
                                     [&out]() { out.clear(); },
                                     minCodeSize,
                                     WRITE_DEFAULT_CHUNK_SIZE);
    decoder.process(data);
    decoder.finish();
    return out;
}