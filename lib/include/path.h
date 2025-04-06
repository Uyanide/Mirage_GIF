#ifndef GIF_HIDE_PIC_PATH_H
#define GIF_HIDE_PIC_PATH_H

#include <filesystem>
#include <string>

#ifdef _WIN32

#include <windows.h>

static inline std::wstring
localizePath(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

static inline std::string
deLocalizePath(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);
    return str;
}

static inline std::string
deLocalizePath(const std::filesystem::path& path) {
    std::wstring wstr = path.wstring();
    int size_needed   = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);
    return str;
}

#else  // _WIN32

static std::string inline localizePath(const std::string& str) { return str; }

static std::string inline deLocalizePath(const std::string& str) { return str; }

static std::string inline deLocalizePath(const std::filesystem::path& path) { return path.string(); }

#endif  // _WIN32

static std::string
getExtName(const std::string& str) {
    auto pos = str.find_last_of('.');
    if (pos == std::string::npos) {
        return str;
    }
    return str.substr(pos + 1);
}

static std::string
getFileName(const std::string& str) {
    auto pos = str.find_last_of('/');
    if (pos == std::string::npos) {
        pos = str.find_last_of('\\');
    }
    if (pos == std::string::npos) {
        return str;
    }
    return str.substr(pos + 1);
}

static inline bool
isValidFileName(const std::string& str) {
    return !str.empty() && str.find_first_of("\\/:*?\"<>|") == std::string::npos;
}

#endif  // GIF_HIDE_PIC_PATH_H