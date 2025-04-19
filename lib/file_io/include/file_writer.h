#ifndef NAIVEIO_FILE_WRITER_H
#define NAIVEIO_FILE_WRITER_H

#include <exception>
#include <memory>
#include <span>
#include <string>


namespace NaiveIO {
class FileWriterException final : public std::exception {
  public:
    explicit FileWriterException(const std::string&& msg)
        : msg(msg) {}

    [[nodiscard]] inline const char*
    what() const noexcept override {
        return msg.c_str();
    }

  private:
    std::string msg;
};

class FileWriter {
  public:
    using Ref = std::unique_ptr<FileWriter>;

    static Ref
    create(const std::string& fileName, const std::string& extName = "") noexcept;

    virtual ~FileWriter() = default;

    virtual bool
    close() noexcept = 0;

    [[nodiscard]] virtual bool
    isOpen() const noexcept = 0;

    [[nodiscard]] virtual size_t
    getWrittenSize() const noexcept = 0;

    virtual size_t
    write(const std::span<const uint8_t>& buffer) = 0;

    virtual size_t
    write(uint8_t byte) = 0;

    virtual bool
    deleteFile() noexcept = 0;

    [[nodiscard]] virtual std::string
    getFilePath() const noexcept = 0;

    virtual bool
    rename(const std::string& newName, bool force = true) noexcept = 0;

    virtual bool
    move(const std::string& newPath, bool force = true) noexcept = 0;
};
}  // namespace NaiveIO
#endif  // NAIVEIO_FILE_WRITER_H