#ifndef FILE_READER_H
#define FILE_READER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace GIFLsb {

class FileReaderException final : public std::exception {
  public:
    explicit FileReaderException(const std::string&& msg)
        : msg(msg) {}

    [[nodiscard]] inline const char*
    what() const noexcept override {
        return msg.c_str();
    }

  private:
    std::string msg;
};

class FileReader {
  public:
    using Ref = std::unique_ptr<FileReader>;

    static Ref
    create(const std::string& fileName) noexcept;

    virtual ~FileReader() = default;

    virtual bool
    close() = 0;

    [[nodiscard]] virtual bool
    isOpen() const = 0;

    [[nodiscard]] virtual bool
    isEOF() const = 0;

    [[nodiscard]] virtual size_t
    getSize() = 0;

    virtual size_t
    read(std::span<uint8_t> buffer) = 0;
};

};  // namespace GIFLsb

#endif  // FILE_READER_H