#ifndef GIF_LZW_H
#define GIF_LZW_H

#include <functional>
#include <span>
#include <vector>

#include "def.h"

namespace GIFEnc {
namespace LZW {
constexpr uint32_t MAX_CODE_SIZE          = 12;
constexpr uint32_t MAX_DICT_SIZE          = 1u << MAX_CODE_SIZE;
constexpr size_t WRITE_DEFAULT_CHUNK_SIZE = 32768;

using WriteCallback = std::function<void(const std::span<uint8_t>&)>;
using ReadCallback  = std::function<std::span<uint8_t>()>;
using ErrorCallback = std::function<void()>;

size_t
compressStream(const ReadCallback& read,
               const WriteCallback& write,
               const ErrorCallback& onError = nullptr,
               uint32_t minCodeSize         = 8,
               size_t writeChunkSize        = WRITE_DEFAULT_CHUNK_SIZE) noexcept;

size_t
decompressStream(const ReadCallback& read,
                 const WriteCallback& write,
                 const ErrorCallback& onError = nullptr,
                 uint32_t minCodeSize         = 8,
                 size_t writeChunkSize        = WRITE_DEFAULT_CHUNK_SIZE) noexcept;

std::vector<uint8_t>
compress(const std::span<uint8_t>& data, uint32_t minCodeSize = 8) noexcept;

std::vector<uint8_t>
decompress(const std::span<uint8_t>& data, uint32_t minCodeSize = 8) noexcept;

};  // namespace LZW

};  // namespace GIFEnc

#endif  // GIF_LZW_H