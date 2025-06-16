#include "file_reader.h"

#include <exception>
#include <filesystem>
#include <fstream>

#include "file_utils.h"
#include "log.h"
using std::string, std::span;

using namespace NaiveIO;

class FileReaderImpl final : public FileReader {
  public:
    FileReaderImpl(const string& path) {
        m_filePath = std::filesystem::path(localizePath(path));
        if (!std::filesystem::exists(m_filePath)) {
            throw FileReaderException("File does not exist: " + path);
        }
        m_file = std::ifstream(m_filePath, std::ios::binary);
        if (!m_file.is_open()) {
            throw FileReaderException("Failed to open input file: " + path);
        }
    }

    [[nodiscard]] bool
    isOpen() const override {
        return m_file.is_open();
    }

    bool
    close() override {
        if (isOpen()) {
            m_file.close();
            return true;
        }
        return false;
    }

    [[nodiscard]] bool
    isEOF() const override {
        return m_file.eof();
    }

    [[nodiscard]] size_t
    getSize() override {
        if (!isOpen()) {
            throw FileReaderException("File is not open.");
        }
        const size_t currPos = m_file.tellg();
        m_file.seekg(0, std::ios::end);
        if (m_file.fail()) {
            throw FileReaderException("Failed to seek to end of file.");
        }
        const auto fileSize = m_file.tellg();
        m_file.seekg(currPos, std::ios::beg);
        if (m_file.fail()) {
            throw FileReaderException("Failed to seek to beginning of file.");
        }
        if (fileSize == -1) {
            throw FileReaderException("Failed to get file size.");
        }
        return static_cast<size_t>(fileSize);
    }

    size_t
    read(std::span<uint8_t>& buffer) override {
        if (!isOpen()) {
            throw FileReaderException("File is not open.");
        }
        if (buffer.empty()) {
            return 0;
        }
        m_file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        if (m_file.fail() && !m_file.eof()) {
            throw FileReaderException("Fatal error while reading file.");
        }
        return static_cast<size_t>(m_file.gcount());
    }

    ~FileReaderImpl() override {
        close();
    }

  private:
    std::filesystem::path m_filePath;
    std::ifstream m_file;
};

FileReader::Ref
FileReader::create(const string& fileName) noexcept {
    try {
        return std::make_unique<FileReaderImpl>(fileName);
    } catch (std::exception& e) {
        GeneralLogger::error("Failed to read file: " + string(e.what()));
    } catch (...) {
        GeneralLogger::error("Failed to read file.");
    }
    return nullptr;
}

class FileReaderMemoryImpl final : public FileReader {
  public:
    FileReaderMemoryImpl(const std::span<uint8_t>& data, const string& fileName)
        : m_data(data), m_fileName(fileName), m_position(0) {}

    [[nodiscard]] bool
    isOpen() const override {
        return true;  // Always open for memory
    }

    bool
    close() override {
        return true;  // No resources to release
    }

    [[nodiscard]] bool
    isEOF() const override {
        return m_position >= m_data.size();
    }

    [[nodiscard]] size_t
    getSize() override {
        return m_data.size();
    }

    size_t
    read(std::span<uint8_t>& buffer) override {
        if (isEOF()) {
            return 0;
        }
        size_t bytesToRead = std::min(buffer.size(), m_data.size() - m_position);
        std::copy(m_data.data() + m_position, m_data.data() + m_position + bytesToRead, buffer.data());
        m_position += bytesToRead;
        return bytesToRead;
    }

  private:
    std::span<uint8_t> m_data;
    string m_fileName;
    size_t m_position;
};

FileReader::Ref
FileReader::createFromMemory(const std::span<uint8_t>& data, const std::string& fileName) noexcept {
    try {
        return std::make_unique<FileReaderMemoryImpl>(data, fileName);
    } catch (std::exception& e) {
        GeneralLogger::error("Failed to read file: " + string(e.what()));
    } catch (...) {
        GeneralLogger::error("Failed to read file.");
    }
    return nullptr;
}
