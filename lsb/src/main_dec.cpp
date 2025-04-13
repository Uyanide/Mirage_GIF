#ifdef _WIN32

#include <windows.h>

#include <cwchar>
#include <string>

// use wmain instead
#define main gifLsbDecMain

#endif  // _WIN32

#include "gif_lsb.h"
#include "imsq_stream.h"
#include "log.h"
#include "options.h"

int
main(int argc, char** argv) {
    auto options = GIFLsb::DecodeOptions::parseArgs(argc, argv);
    if (!options) {
        return 1;
    }
    if (!GIFImage::ImageSequenceStream::initDecoder(argv[0])) {
        return 1;
    }
    if (!GIFLsb::gifLsbDecode(*options)) {
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

std::ostream* GeneralLogger::logStream = nullptr;

void
GeneralLogger::initLogStream() noexcept {
    if (GeneralLogger::logStream == nullptr) {
        GeneralLogger::logStream = &std::cerr;
    }
}
#ifdef MOCK_COMMAND_LINE

#if defined(__GNUC__)
#warning "Mocking command line arguments."
#else
#pragma message("Mocking command line arguments.")
#endif

#define ARGC 6
#define ARGLEN 255

char**
mockCommandLine() {
    char** argv             = new char*[ARGC];
    char args[ARGC][ARGLEN] = {
        "only useful when initializing Magick++",
        "../../images/encrypted.gif",
        "-o",
        "decrypted",
        "-d",
        "../../images",
    };
    GeneralLogger::warning("Mocking command line arguments.");
    for (int i = 0; i < ARGC; ++i) {
        argv[i] = new char[ARGLEN];
        strcpy(argv[i], args[i]);
        GeneralLogger::warning(args[i], GeneralLogger::STEP);
    }
    return argv;
    // will be freed in main()
}

#endif  // MOCK_COMMAND_LINE

int
wmain(int argc, wchar_t** wargv) {
    SetConsoleOutputCP(CP_UTF8);

#ifdef MOCK_COMMAND_LINE
    argc        = ARGC;
    char** argv = mockCommandLine();
#else
    char** argv = new char*[argc];
    for (int i = 0; i < argc; ++i) {
        argv[i] = wstrToUtf8(wargv[i]);
    }
#endif  // MOCK_COMMAND_LINE

    const auto ret = gifLsbDecMain(argc, argv);
    for (int i = 0; i < argc; ++i) {
        delete[] argv[i];
    }
    delete[] argv;
    return ret;
}

#endif  // _WIN32
