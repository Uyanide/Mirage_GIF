#include "file_writer.h"

#include <filesystem>
#include <fstream>

#include "file_utils.h"
#include "log.h"

using namespace NaiveIO;

class FileWriterImpl final : public NaiveIO::FileWriter {
  public:
    explicit FileWriterImpl(const std::string& path, const std::string& extName) {
        m_filePath = std::filesystem::path(localizePath(path));
        if (!extName.empty()) {
            m_filePath.replace_extension(extName);
        }
        m_file.open(m_filePath, std::ios::binary | std::ios::trunc);
        if (!m_file.is_open()) {
            throw FileWriterException("Failed to open output file: " + path);
        }
    }

    ~FileWriterImpl() override {
        close();
    }

    bool
    close() noexcept override {
        if (isOpen()) {
            m_file.close();
            return true;
        }
        return false;
    }

    [[nodiscard]] bool
    isOpen() const noexcept override {
        return m_file.is_open();
    }

    [[nodiscard]] size_t
    getWrittenSize() const noexcept override {
        return m_writtenSize;
    }

    size_t
    write(const std::span<const uint8_t>& buffer) override {
        if (!isOpen()) {
            throw FileWriterException("File is not open.");
        }
        m_file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        if (m_file.fail()) {
            throw FileWriterException("Failed to write to file.");
        }
        m_writtenSize += buffer.size();
        return buffer.size();
    }

    size_t
    write(uint8_t byte) override {
        if (!isOpen()) {
            throw FileWriterException("File is not open.");
        }
        m_file.write(reinterpret_cast<const char*>(&byte), sizeof(byte));
        if (m_file.fail()) {
            throw FileWriterException("Failed to write to file.");
        }
        ++m_writtenSize;
        return 1;
    }

    bool
    deleteFile() noexcept override {
        if (isOpen()) {
            close();
        }
        if (std::filesystem::exists(m_filePath)) {
            return std::filesystem::remove(m_filePath);
        }
        return false;
    }

    [[nodiscard]] std::string
    getFilePath() const noexcept override {
        return deLocalizePath(m_filePath.string());
    }

    bool
    rename(const std::string& name, bool force) noexcept override {
        if (isOpen()) return false;
        if (name.empty()) return false;
        if (m_filePath.empty()) return false;
        if (!std::filesystem::exists(m_filePath)) return false;

        const auto newName = localizePath(name);
        const auto newPath =
            std::filesystem::path(localizePath(m_filePath.parent_path() / newName));

        if (std::filesystem::exists(newPath)) {
            if (force) {
                if (!std::filesystem::remove(newPath)) {
                    return false;
                }
            } else {
                return false;
            }
        }
        if (std::filesystem::exists(m_filePath)) {
            std::filesystem::rename(m_filePath, newPath);
            m_filePath = newPath;
            return true;
        }
        return false;
    }

    bool
    move(const std::string& path, bool force) noexcept override {
        if (isOpen()) return false;
        if (path.empty()) return false;
        if (m_filePath.empty()) return false;
        if (!std::filesystem::exists(m_filePath)) return false;

        const auto newPath = std::filesystem::path(localizePath(path));
        if (std::filesystem::exists(newPath)) {
            if (force) {
                if (!std::filesystem::remove(newPath)) {
                    return false;
                }
            } else {
                return false;
            }
        }
        if (std::filesystem::exists(m_filePath)) {
            std::filesystem::rename(m_filePath, newPath);
            m_filePath = newPath;
            return true;
        }
        return false;
    }

  private:
    std::filesystem::path m_filePath;
    std::ofstream m_file;
    size_t m_writtenSize = 0;
};

NaiveIO::FileWriter::Ref
NaiveIO::FileWriter::create(const std::string& fileName, const std::string& extName) noexcept {
    try {
        return std::make_unique<FileWriterImpl>(fileName, extName);
    } catch (const FileWriterException& e) {
        GeneralLogger::error("Failed to create file: " + std::string(e.what()));
        return nullptr;
    }
}