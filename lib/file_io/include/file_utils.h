#ifndef NAIVEIO_FILE_UTILS_H
#define NAIVEIO_FILE_UTILS_H

#include <filesystem>
#include <string>

#ifdef _WIN32

#include <windows.h>
#endif

namespace NaiveIO {

#ifdef _WIN32

inline std::wstring
localizePath(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
    return wstr;
}

inline std::string
deLocalizePath(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);
    return str;
}

inline std::string
deLocalizePath(const std::filesystem::path& path) {
    std::wstring wstr = path.wstring();
    int size_needed   = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);
    return str;
}

#else  // _WIN32

inline std::string
localizePath(const std::string& str) {
    return str;
}

inline std::string
deLocalizePath(const std::string& str) {
    return str;
}

inline std::string
deLocalizePath(const std::filesystem::path& path) {
    return path.string();
}

#endif  // _WIN32

inline std::string
getExtName(const std::string& str) {
    auto pos = str.find_last_of('.');
    if (pos == std::string::npos) {
        return ".dat";
    }
    return str.substr(pos);
}

inline std::string
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

inline std::string
replaceExtName(const std::string& str, const std::string& extName) {
    if (extName.empty()) {
        return str;
    }
    std::string ext;
    if (extName[0] != '.') {
        ext = '.' + extName;
    } else {
        ext = extName;
    }
    auto pos = str.find_last_of('.');
    if (pos == std::string::npos) {
        return str + ext;
    }
    return str.substr(0, pos) + ext;
}

inline bool
isValidFileName(const std::string& str) {
    return !str.empty() && str.find_first_of("\\/:*?\"<>|") == std::string::npos;
}

inline auto
checkFileExists(const std::string& filename) {
    auto localized = NaiveIO::localizePath(filename);
    if (const std::filesystem::path filePath(localized); std::filesystem::exists(filePath)) {
        return localized;
    } else {
        return decltype(localized)();
    }
}

}  // namespace NaiveIO

#endif  // NAIVEIO_FILE_UTILS_H