#ifdef _WIN32

#include <windows.h>

#include <cwchar>
#include <string>

// use wmain instead
#define main gifMirageMain

#endif  // _WIN32

#include "gif_mirage.h"
#include "gif_options.h"
#include "imsq.h"

#ifdef MOCK_COMMAND_LINE

#include <cstring>

#include "log.h"

#if defined(__GNUC__)
#warning "Mocking command line arguments."
#else
#pragma message("Mocking command line arguments.")
#endif

#define ARGC 11
#define ARGLEN 255

char**
mockCommandLine() {
    char** argv             = new char*[ARGC];
    char args[ARGC][ARGLEN] = {
        "only useful when initializing Magick++",
        "../../images/气气.gif",
        "../../images/马达.gif",
        "-o",
        "../../images/mirage-output.gif",
        "-x",
        "2048",
        "-y",
        "2048",
        "-f",
        "1000",
    };
    GeneralLogger::warn("Mocking command line arguments.");
    for (int i = 0; i < ARGC; ++i) {
        argv[i] = new char[ARGLEN];
        strcpy(argv[i], args[i]);
        GeneralLogger::warn(args[i], GeneralLogger::STEP);
    }
    return argv;
}

#endif  // MOCK_COMMAND_LINE

int
main(int argc, char** argv) {

#ifdef MOCK_COMMAND_LINE
    argc = ARGC;
    argv = mockCommandLine();
#endif  // MOCK_COMMAND_LINE

    auto options = GIFMirage::Options::parseArgs(argc, argv);
    if (!options) {
        return 1;
    }
    if (!GIFImage::ImageSequence::initDecoder(argv[0])) {
        return 1;
    }
    if (!GIFMirage::gifMirageEncode(*options)) {
        return 1;
    }
    return 0;
}

#ifdef _WIN32
#undef main

char*
wstrToUtf8(const wchar_t* wstr) {
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    char* str       = new char[size_needed + 1];
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, size_needed, nullptr, nullptr);
    str[size_needed] = '\0';
    return str;
}

int
wmain(int argc, wchar_t** wargv) {
    SetConsoleOutputCP(CP_UTF8);

    char** argv = new char*[argc];
    for (int i = 0; i < argc; ++i) {
        argv[i] = wstrToUtf8(wargv[i]);
    }

    const auto ret = gifMirageMain(argc, argv);
    for (int i = 0; i < argc; ++i) {
        delete[] argv[i];
    }
    delete[] argv;
    return ret;
}

#endif  // _WIN32