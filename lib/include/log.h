#ifndef GENERAL_LOGGER_H
#define GENERAL_LOGGER_H

#include <iostream>
#include <mutex>

#include "def.h"

namespace GeneralLogger {
static std::mutex logMutex;

static constexpr const char* colorInfoMsg[]{"\033[32m", "\033[0m", "\033[0m"};

enum LogIndent : uint32_t {
    GENERAL = 0,
    STEP    = 1,
    DETAIL  = 2,
};

inline void
info(const std::string_view& msg,
     const LogIndent indent  = GENERAL,
     const bool color        = true,
     std::ostream* logStream = &std::cerr) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logStream == nullptr) return;

    *logStream << (color ? "\033[92m" : "") << "[INFO] ";
    for (uint32_t i = 0; i < indent; i++) *logStream << "  ";
    *logStream << colorInfoMsg[indent] << msg << (color ? "\033[0m\n" : "\n");
}

inline void
warn(const std::string_view& msg,
     const LogIndent indent  = GENERAL,
     const bool color        = true,
     std::ostream* logStream = &std::cerr) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logStream == nullptr) return;

    *logStream << (color ? "\033[93m" : "") << "[WARN] ";
    for (uint32_t i = 0; i < indent; i++) *logStream << "  ";
    *logStream << (color ? "\033[33m" : "") << msg << (color ? "\033[0m\n" : "\n");
}

inline void
error(const std::string_view& msg,
      const LogIndent indent  = GENERAL,
      const bool color        = true,
      std::ostream* logStream = &std::cerr) {
    std::lock_guard<std::mutex> lock(logMutex);
    if (logStream == nullptr) return;

    *logStream << (color ? "\033[91m" : "") << "[ERROR] ";
    for (uint32_t i = 0; i < indent; i++) *logStream << "  ";
    *logStream << (color ? "\033[31m" : "") << msg << (color ? "\033[0m\n" : "\n");
}
};  // namespace GeneralLogger

#endif  // GENERAL_LOGGER_H