#ifndef ENTRY_UTILS_H
#define ENTRY_UTILS_H

#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

#include "log.h"

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

namespace CLIUtils {

class CLIArgs {
  public:
    explicit CLIArgs(const std::vector<std::string>& args) {
        m_argc = args.size();
        m_argv = new char*[m_argc];
        for (int i = 0; i < m_argc; ++i) {
            m_argv[i] = new char[args[i].length() + 1];
            std::strcpy(m_argv[i], args[i].c_str());
        }
    }

#ifdef _WIN32

    CLIArgs(int argc, wchar_t** wargv) : m_argc(argc) {
        m_argc = argc;
        m_argv = new char*[m_argc];
        for (int i = 0; i < m_argc; ++i) {
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
            m_argv[i]       = new char[size_needed + 1];
            WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, m_argv[i], size_needed, nullptr, nullptr);
            m_argv[i][size_needed] = '\0';
        }
    }

#endif  // _WIN32

    ~CLIArgs() {
        for (int i = 0; i < m_argc; ++i) {
            delete[] m_argv[i];
        }
        delete[] m_argv;
    }

    operator char**() {
        return m_argv;
    }

    [[nodiscard]] inline int
    argc() const {
        return m_argc;
    }

    [[nodiscard]] inline char**
    argv() const {
        return m_argv;
    }

  private:
    int m_argc    = 0;
    char** m_argv = nullptr;
};

inline CLIUtils::CLIArgs
mockCommandLine(std::vector<std::string> args) {
    auto ret = CLIUtils::CLIArgs(args);
    GeneralLogger::warn("Mocking command line arguments.");
    for (int i = 0; i < ret.argc(); ++i) {
        GeneralLogger::warn(ret[i], GeneralLogger::STEP);
    }
    return ret;
}

}  // namespace CLIUtils

#ifdef CLI_MAIN

#ifdef _WIN32

#define main main_

int
main(int, char**);

int
wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    CLIUtils::CLIArgs args(argc, argv);
    return main(args.argc(), args.argv());
}

#endif  // _WIN32

#endif  // CLI_UTILS_HANDLE_MAIN

#endif  // ENTRY_UTILS_H