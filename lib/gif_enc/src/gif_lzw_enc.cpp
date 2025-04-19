#include <cstring>
#include <utility>

#include "gif_lzw.h"
using std::vector, std::span;

class LZWCompressImpl {
    // static constexpr uint32_t MAX_DATA = 4;  // 0~3, enough for me :)
    static constexpr uint8_t MAX_DATA        = 255;  // but 256 is better for general use
    static constexpr uint32_t MAX_CHUNK_SIZE = 255;
    using LZWNode                            = uint16_t[MAX_DATA + 1];  // index: data; value: pointer to next node

  public:
    LZWCompressImpl(const GIFEnc::LZW::WriteCallback& write,
                    const GIFEnc::LZW::ErrorCallback& onError,
                    uint32_t minCodeSize,
                    size_t writeChunkSize);
    ~LZWCompressImpl();

    void
    process(const span<const uint8_t>& input);
    size_t
    finish();

    [[nodiscard]] bool
    isFinished() const {
        return m_isFinished;
    }

  private:
    void
    _pushCode(uint16_t code);
    void
    _reset();
    void
    _onError();

    vector<uint8_t> m_result;
    const size_t m_writeChunkSize;
    size_t m_resultSize      = 0;
    size_t m_resultTotalSize = 0;
    const GIFEnc::LZW::WriteCallback& m_write;
    const GIFEnc::LZW::ErrorCallback& m_onError;
    const uint32_t m_minCodeSize;  // the code before each LZW chunk

    uint16_t m_maxCode = 0, m_nextCode = 0;
    const uint16_t m_clearCode, m_endCode;
    uint32_t m_codeLength = 0;
    uint32_t m_buffer = 0, m_bufferSize = 0;  // byte buffer

    LZWNode* m_dict     = nullptr;  // memory pool. index: code + 1; value: node. 0 is reserved for null
    uint16_t m_currNode = 0;        // pointer to current node

    bool m_isFinished = false;
};

LZWCompressImpl::LZWCompressImpl(const GIFEnc::LZW::WriteCallback& write,
                                 const GIFEnc::LZW::ErrorCallback& onError,
                                 const uint32_t minCodeSize,
                                 const size_t writeChunkSize)
    : m_writeChunkSize(writeChunkSize),
      m_write(std::move(write)),
      m_onError(std::move(onError)),
      m_minCodeSize(minCodeSize),
      m_clearCode(1 << minCodeSize),
      m_endCode(m_clearCode + 1) {
    m_result.resize(writeChunkSize);
    m_dict = new LZWNode[GIFEnc::LZW::MAX_DICT_SIZE + 1];
    _reset();
    _pushCode(m_clearCode);
}

LZWCompressImpl::~LZWCompressImpl() {
    delete[] m_dict;
}

void
LZWCompressImpl::process(const span<const uint8_t>& input) {
    if (m_isFinished) {
        return;
    }
    for (size_t i = 0; i < input.size(); ++i) {
        const uint8_t& data = input[i];
        if (data >= 1 << m_minCodeSize) {
            _onError();
            return;
        }
        if (!m_currNode) {  // first data
            m_currNode = data + 1;
        } else {
            if (uint16_t& nextNode = m_dict[m_currNode][data]) {  // next node exists
                m_currNode = nextNode;
            } else {
                _pushCode(m_currNode - 1);
                if (m_nextCode < GIFEnc::LZW::MAX_DICT_SIZE) {
                    nextNode = m_nextCode + 1;  // create new node
                    if (m_nextCode >= m_maxCode) {
                        m_maxCode <<= 1;
                        ++m_codeLength;
                    }
                    m_currNode = data + 1;  // reset current node to root nodes
                    ++m_nextCode;
                } else {  // reach max code length
                    _pushCode(m_clearCode);
                    _reset();
                    i--;
                }
            }
        }
    }
}

size_t
LZWCompressImpl::finish() {
    if (m_isFinished) return 0;
    m_isFinished = true;

    if (m_currNode)  // push last node
        _pushCode(m_currNode - 1);
    _pushCode(m_endCode);
    if (m_bufferSize) {
        m_result[m_resultSize++] = static_cast<uint8_t>(m_buffer);
    }

    if (m_resultSize) {
        m_write(span<uint8_t>(m_result.data(), m_resultSize));
        m_resultTotalSize += m_resultSize;
        m_resultSize = 0;
    }

    return m_resultTotalSize;
}

void
LZWCompressImpl::_reset() {
    memset(m_dict + 1, 0, sizeof(LZWNode) * GIFEnc::LZW::MAX_DICT_SIZE);
    m_currNode   = 0;
    m_nextCode   = m_endCode + 1;
    m_maxCode    = 1 << (m_minCodeSize + 1);
    m_codeLength = m_minCodeSize + 1;
}

void
LZWCompressImpl::_pushCode(uint16_t code) {
    m_buffer |= code << m_bufferSize;
    m_bufferSize += m_codeLength;
    while (m_bufferSize >= 8) {
        m_result[m_resultSize++] = static_cast<uint8_t>(m_buffer & 0xFF);
        m_buffer >>= 8;
        m_bufferSize -= 8;

        if (m_resultSize >= m_writeChunkSize) {
            m_write(span<uint8_t>(m_result.data(), m_resultSize));
            m_resultTotalSize += m_resultSize;
            m_resultSize = 0;
        }
    }
}

void
LZWCompressImpl::_onError() {
    m_isFinished = true;
    m_resultSize = 0;
    _reset();
    if (m_onError) {
        m_onError();
    }
}

size_t
GIFEnc::LZW::compressStream(const ReadCallback& read,
                            const GIFEnc::LZW::WriteCallback& write,
                            const GIFEnc::LZW::ErrorCallback& onError,
                            uint32_t minCodeSize,
                            size_t writeChunkSize) noexcept {
    if (minCodeSize < 2) {
        return 0;
    }
    if (read == nullptr || write == nullptr) {
        return 0;
    }
    vector<uint8_t> out;
    auto encoder = LZWCompressImpl(write, onError, minCodeSize, writeChunkSize);
    while (true) {
        auto data = read();
        if (data.empty()) break;
        encoder.process(data);
    }
    return encoder.finish();
}

vector<uint8_t>
GIFEnc::LZW::compress(const span<const uint8_t>& data, uint32_t minCodeSize) noexcept {
    if (minCodeSize < 2) {
        return {};
    }
    vector<uint8_t> out;
    auto encoder = LZWCompressImpl([&out](const span<const uint8_t>& data) { out.insert(out.end(), data.begin(), data.end()); },
                                   [&out]() { out.clear(); },
                                   minCodeSize,
                                   GIFEnc::LZW::WRITE_DEFAULT_CHUNK_SIZE);
    encoder.process(data);
    encoder.finish();
    return out;
}