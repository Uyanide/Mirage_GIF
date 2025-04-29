#ifndef ENTRY_UTILS_H
#define ENTRY_UTILS_H

#include <cstring>
#include <string>
#include <vector>

#include "log.h"

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

namespace CLIUtils {

#ifdef _WIN32

void
wstr2str(const wchar_t* wstr, char*& str) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    str             = new char[size_needed]{};
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, size_needed, nullptr, nullptr);
}

#endif  // _WIN32

class CLIArgs {
  public:
    explicit CLIArgs(char* arg0, const std::vector<std::string>& args) {
        m_argc    = args.size() + 1;
        m_argv    = new char*[m_argc];
        m_argv[0] = new char[strlen(arg0) + 1];
        std::strcpy(m_argv[0], arg0);
        for (int i = 1; i < m_argc; ++i) {
            m_argv[i] = new char[args[i - 1].length() + 1];
            std::strcpy(m_argv[i], args[i - 1].c_str());
        }
    }

#if defined(_WIN32)

    CLIArgs(int argc, wchar_t** wargv)
        : m_argc(argc) {
        m_argc = argc;
        m_argv = new char*[m_argc];
        for (int i = 0; i < m_argc; ++i) {
            wstr2str(wargv[i], m_argv[i]);
        }
    }

#endif  // _WIN32

    ~CLIArgs() {
        for (int i = 0; i < m_argc; ++i) {
            delete[] m_argv[i];
        }
        delete[] m_argv;
    }

    operator char**() { return m_argv; }

    [[nodiscard]] inline int
    argc() const { return m_argc; }

    [[nodiscard]] inline char**
    argv() const { return m_argv; }

  private:
    int m_argc    = 0;
    char** m_argv = nullptr;
};

}  // namespace CLIUtils

#ifdef CLI_MAIN_MOCK

/**
 * @brief Mocked command line arguments for testing purposes.
 * @note This is a placeholder and should be replaced with actual arguments.
 * @note Not including the first argument (program name)
 */
extern const std::vector<std::string> g_mockArgs;

#endif  // CLI_MAIN_MOCK

#ifdef CLI_MAIN

// main_orig: main function defined in .cpp source file
// main_handled: wrap main_orig if needed
// main_mocked: implementation of main_handled, mocks command line args
// wmain / main: actual entry point

int
main_orig(int, char**);

#ifdef CLI_MAIN_MOCK

#define main_handled main_mocked

int
main_mocked(int argc, char** argv) {
    auto ret = CLIUtils::CLIArgs(argv[0], g_mockArgs);
    GeneralLogger::warn("Mocking command line arguments.");
    for (const auto& arg : g_mockArgs) {
        GeneralLogger::warn(arg, GeneralLogger::STEP);
    }
    return main_orig(ret.argc(), ret.argv());
}

#else  // CLI_MAIN_MOCK

#define main_handled main_orig

#endif  // CLI_MAIN_MOCK

#ifdef _WIN32

int
wmain(int argc, wchar_t** argv) {
    SetConsoleOutputCP(CP_UTF8);
    CLIUtils::CLIArgs args(argc, argv);
    return main_handled(args.argc(), args.argv());
}

#else  // _WIN32

int
main(int argc, char** argv) {
    return main_handled(argc, argv);
}

#endif  // _WIN32

#define main main_orig

#endif  // CLI_MAIN

#endif  // ENTRY_UTILS_H