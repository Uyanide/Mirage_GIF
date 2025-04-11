#include <cstring>
#include <utility>

#include "gif_lzw.h"
using std::vector, std::span;

class LZWCompressImpl {
    // static constexpr u32 MAX_DATA = 4;  // 0~3, enough for me :)
    static constexpr u8 MAX_DATA        = 255;  // but 256 is better for general use
    static constexpr u32 MAX_CHUNK_SIZE = 255;
    using LZWNode                       = u16[MAX_DATA + 1];  // index: data; value: pointer to next node

   public:
    LZWCompressImpl(const GIFEnc::LZW::WriteCallback& write,
                    const GIFEnc::LZW::ErrorCallback& onError,
                    u32 minCodeSize,
                    size_t writeChunkSize);
    ~LZWCompressImpl();

    void
    process(const span<u8>& input);
    size_t
    finish();
    [[nodiscard]] bool
    isFinished() const {
        return m_isFinished;
    }

   private:
    void
    _pushCode(u16 code);
    void
    _reset();
    void
    _onError();

    vector<u8> m_result;
    const size_t m_writeChunkSize;
    size_t m_resultSize      = 0;
    size_t m_resultTotalSize = 0;
    const GIFEnc::LZW::WriteCallback& m_write;
    const GIFEnc::LZW::ErrorCallback& m_onError;
    const u32 m_minCodeSize;  // the code before each LZW chunk

    u16 m_maxCode = 0, m_nextCode = 0;
    const u16 m_clearCode, m_endCode;
    u32 m_codeLength = 0;
    u32 m_buffer = 0, m_bufferSize = 0;  // byte buffer

    LZWNode* m_dict = nullptr;  // memory pool. index: code + 1; value: node. 0 is reserved for null
    u16 m_currNode  = 0;        // pointer to current node

    bool m_isFinished = false;
};

LZWCompressImpl::LZWCompressImpl(const GIFEnc::LZW::WriteCallback& write,
                                 const GIFEnc::LZW::ErrorCallback& onError,
                                 const u32 minCodeSize,
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
LZWCompressImpl::process(const span<u8>& input) {
    if (m_isFinished) {
        return;
    }
    for (size_t i = 0; i < input.size(); ++i) {
        const u8& data = input[i];
        if (data >= 1 << m_minCodeSize) {
            _onError();
            return;
        }
        if (!m_currNode) {  // first data
            m_currNode = data + 1;
        } else {
            if (u16& nextNode = m_dict[m_currNode][data]) {  // next node exists
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
        m_result[m_resultSize++] = static_cast<u8>(m_buffer);
    }

    if (m_resultSize) {
        m_write(span<u8>(m_result.data(), m_resultSize));
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
LZWCompressImpl::_pushCode(u16 code) {
    m_buffer |= code << m_bufferSize;
    m_bufferSize += m_codeLength;
    while (m_bufferSize >= 8) {
        m_result[m_resultSize++] = static_cast<u8>(m_buffer & 0xFF);
        m_buffer >>= 8;
        m_bufferSize -= 8;

        if (m_resultSize >= m_writeChunkSize) {
            m_write(span<u8>(m_result.data(), m_resultSize));
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
                            u32 minCodeSize,
                            size_t writeChunkSize) noexcept {
    if (minCodeSize < 2) {
        return 0;
    }
    if (read == nullptr || write == nullptr) {
        return 0;
    }
    vector<u8> out;
    auto encoder = LZWCompressImpl(write, onError, minCodeSize, writeChunkSize);
    while (true) {
        auto data = read();
        if (data.empty()) break;
        encoder.process(data);
    }
    return encoder.finish();
}

vector<u8>
GIFEnc::LZW::compress(const span<u8>& data, u32 minCodeSize) noexcept {
    if (minCodeSize < 2) {
        return {};
    }
    vector<u8> out;
    auto encoder = LZWCompressImpl([&out](const span<u8>& data) { out.insert(out.end(), data.begin(), data.end()); },
                                   [&out]() { out.clear(); },
                                   minCodeSize,
                                   GIFEnc::LZW::WRITE_DEFAULT_CHUNK_SIZE);
    encoder.process(data);
    encoder.finish();
    return out;
}