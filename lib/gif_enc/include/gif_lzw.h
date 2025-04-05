#ifndef GIF_LZW_H
#define GIF_LZW_H

#include <functional>
#include <span>
#include <vector>

#include "def.h"

namespace GIFEnc {
namespace LZW {
constexpr u32 MAX_CODE_SIZE               = 12;
constexpr u32 MAX_DICT_SIZE               = 1u << MAX_CODE_SIZE;
constexpr size_t WRITE_DEFAULT_CHUNK_SIZE = 32768;

using WriteCallback = std::function<void(const std::span<u8>&)>;
using ReadCallback  = std::function<std::span<u8>()>;
using ErrorCallback = std::function<void()>;

size_t
compressStream(const ReadCallback& read,
               const WriteCallback& write,
               const ErrorCallback& onError = nullptr,
               u32 minCodeSize              = 8,
               size_t writeChunkSize        = WRITE_DEFAULT_CHUNK_SIZE) noexcept;

size_t
decompressStream(const ReadCallback& read,
                 const WriteCallback& write,
                 const ErrorCallback& onError = nullptr,
                 u32 minCodeSize              = 8,
                 size_t writeChunkSize        = WRITE_DEFAULT_CHUNK_SIZE) noexcept;

std::vector<u8>
compress(const std::span<u8>& data, u32 minCodeSize = 8) noexcept;

std::vector<u8>
decompress(const std::span<u8>& data, u32 minCodeSize = 8) noexcept;

};  // namespace LZW

};  // namespace GIFEnc

#endif  // GIF_LZW_H