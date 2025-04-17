#include "gif_options.h"

#include <thread>

#include "cxxopts.hpp"
#include "log.h"

using namespace GIFMirage;
using std::string;

static constexpr uint32_t DEFAULT_THREADS = 4;

static uint32_t
getThreadCount() {
    uint32_t threadCount = std::thread::hardware_concurrency();
    return threadCount == 0 ? 1 : (threadCount > DEFAULT_THREADS ? DEFAULT_THREADS : threadCount);
}

class OptionInvalidException final : public std::exception {
  public:
    explicit OptionInvalidException(const std::string&& msg)
        : msg(msg) {}

    [[nodiscard]] const char*
    what() const noexcept override {
        return msg.c_str();
    }

  private:
    std::string msg;
};

std::optional<Options>
Options::parseArgs(int argc, char** argv) noexcept {
    cxxopts::Options options("GIFMirage", "GIF Mirage Generator");

    options.add_options()
        //
        ("inner", "Inner image file", cxxopts::value<string>())
        //
        ("cover", "Cover image file", cxxopts::value<string>())
        //
        ("o,output", "Output GIF file.", cxxopts::value<string>()->default_value(Defaults::outputFile))
        //
        ("x,width",
         "Width of the generated GIF. Max: " + std::to_string(Limits::width),
         cxxopts::value<uint32_t>()->default_value(std::to_string(Defaults::width)))
        //
        ("y,height",
         "Height of the generated GIF. Max: " + std::to_string(Limits::height),
         cxxopts::value<uint32_t>()->default_value(std::to_string(Defaults::height)))
        //
        ("f,frames",
         "Number of frames in the generated GIF. Max: " + std::to_string(Limits::frameCount),
         cxxopts::value<uint32_t>()->default_value(std::to_string(Defaults::frameCount)))
        //
        ("d,duration",
         "Frame duration between frames in milliseconds in the generated GIF. Max: " + std::to_string(Limits::delay),
         cxxopts::value<uint32_t>()->default_value(std::to_string(Defaults::delay)))
        //
        ("s,disposal",
         "Disposal method for the generated GIF. 0 = Not specified, 1 = No disposal, 2 = Background, 3 = Previous.",
         cxxopts::value<uint32_t>()->default_value(std::to_string(Defaults::disposalMethod)))
        //
        ("p,threads",
         "Number of threads to use for processing, 0 = auto-detect.",
         cxxopts::value<uint32_t>()->default_value(std::to_string(Defaults::threadCount)))
        //
        ("m,mode", mergeModeHint, cxxopts::value<string>()->default_value(Defaults::mergeMode))
        //
        ("h,help", "Show help message");

    options.positional_help("<inner-image> <cover-image>");
    options.parse_positional({"inner", "cover"});

    try {
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return std::nullopt;
        }

        if (!result.count("inner") || !result.count("cover")) {
            throw OptionInvalidException("'inner' and 'cover' arguments are required.");
        }

        const auto modeRef = GIFMirage::MergeMode::parse(result["mode"].as<string>());
        if (!modeRef) {
            throw OptionInvalidException("Invalid merge mode: " + result["mode"].as<string>());
        }
        const auto& mode = *modeRef;

        Options gifOptions;
        gifOptions.innerFile      = result["inner"].as<string>();
        gifOptions.coverFile      = result["cover"].as<string>();
        gifOptions.outputFile     = result["output"].as<string>();
        gifOptions.width          = result["width"].as<uint32_t>();
        gifOptions.height         = result["height"].as<uint32_t>();
        gifOptions.frameCount     = result["frames"].as<uint32_t>();
        gifOptions.delay          = result["duration"].as<uint32_t>();
        gifOptions.mergeMode      = mode;
        gifOptions.threadCount    = result["threads"].as<uint32_t>();
        gifOptions.disposalMethod = result["disposal"].as<uint32_t>();

        if (gifOptions.threadCount == 0) {
            gifOptions.threadCount = getThreadCount();
        }

        gifOptions.checkValid();

        return gifOptions;
    } catch (const cxxopts::exceptions::parsing& e) {
        GeneralLogger::error("Error parsing command line arguments: " + string(e.what()));
        std::cout << options.help() << std::endl;
        return std::nullopt;
    } catch (const OptionInvalidException& e) {
        GeneralLogger::error("Invalid argument: " + string(e.what()));
        std::cout << options.help() << std::endl;
        return std::nullopt;
    } catch (const std::exception& e) {
        GeneralLogger::error("Unexpected error: " + string(e.what()));
        std::cout << options.help() << std::endl;
        return std::nullopt;
    } catch (...) {
        GeneralLogger::error("Unexpected error.");
        std::cout << options.help() << std::endl;
        return std::nullopt;
    }
}

void
Options::checkValid() const {
    if (innerFile.empty()) {
        throw OptionInvalidException("Inner image file is required.");
    }
    if (coverFile.empty()) {
        throw OptionInvalidException("Cover image file is required.");
    }
    if (outputFile.empty()) {
        throw OptionInvalidException("Output file is required.");
    }
    if (width == 0 || height == 0) {
        throw OptionInvalidException("Width and height must be positive integers.");
    }
    if (width > Limits::width || height > Limits::height) {
        throw OptionInvalidException("Width and height must be less than " + std::to_string(Limits::width) + ".");
    }
    if (frameCount == 0) {
        throw OptionInvalidException("Frame count must be positive.");
    }
    if (frameCount > Limits::frameCount) {
        throw OptionInvalidException("Frame count must be less than " + std::to_string(Limits::frameCount) + ".");
    }
    if (delay == 0) {
        throw OptionInvalidException("Delay must be positive.");
    }
    if (delay > Limits::delay) {
        throw OptionInvalidException("Delay must be less than " + std::to_string(Limits::delay) + ".");
    }
    if (disposalMethod > Limits::disposalMethod) {
        throw OptionInvalidException("Disposal method must be less than " + std::to_string(Limits::disposalMethod) +
                                     ".");
    }
}

std::optional<MergeMode>
MergeMode::parse(const std::string& str) noexcept {
    MergeMode mode;
    if (str.size() < 5 || str[0] != 'S' || str[2] != 'W') {
        return std::nullopt;
    }

    try {
        mode.slope = std::stoi(str.substr(1, 1));
        mode.width = std::stoi(str.substr(3, 1));
        mode.isRow = (str[4] == 'R');

    } catch (...) {
        return std::nullopt;
    }

    if (mode.slope > GIFMirage::Options::Limits::modeSlope || mode.width > GIFMirage::Options::Limits::modeWidth ||
        mode.width == 0) {
        return std::nullopt;
    }

    return mode;
}