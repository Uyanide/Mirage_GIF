#include <thread>

#include "cxxopts.hpp"
#include "log.h"
#include "options.h"

using namespace GIFLsb;
using std::string;

static constexpr u32 DEFAULT_THREADS = 4;

static u32
getThreadCount() {
    u32 threadCount = std::thread::hardware_concurrency();
    return threadCount == 0 ? 1 : (threadCount > DEFAULT_THREADS ? DEFAULT_THREADS : threadCount);
}

class OptionInvalidException final : public std::exception {
  public:
    explicit OptionInvalidException(const std::string&& msg) : msg(msg) {}
    [[nodiscard]] const char*
    what() const noexcept override {
        return msg.c_str();
    }

  private:
    std::string msg;
};

std::optional<EncodeOptions>
EncodeOptions::parseArgs(int argc, char** argv) noexcept {
    cxxopts::Options options("GIFLsb", "GIF LSB Encoder");

    options.add_options()
        //
        ("image", "Cover image", cxxopts::value<string>())
        //
        ("file", "File to encrypt", cxxopts::value<string>())
        //
        ("o,output", "Output GIF file.", cxxopts::value<string>()->default_value(Defaults::OUTPUT_FILE))
        //
        ("m,mark_text",
         "Marker text to be embedded in the GIF. Set to \"none\" to disable",
         cxxopts::value<string>()->default_value(Defaults::MARK_TEXT))
        //
        ("c,colors",
         "Number of colors in the generated GIF (" + std::to_string(Limits::MIN_NUM_COLORS) + "-" +
             std::to_string(Limits::MAX_NUM_COLORS) + ").",
         cxxopts::value<u32>()->default_value(std::to_string(Defaults::NUM_COLORS)))
        //
        ("g,grayscale", "Use grayscale palette.")
        //
        ("d,no_dither", "Disable dithering.")
        //
        ("t,transparency", "Use transparency.")
        //
        ("l,local_palette", "Use local palette. If enabled, each frame will have its own palette.")
        //
        ("a,threshold",
         "Transparency threshold (0-255), pixels with a alpha value below this will be set transparent.",
         cxxopts::value<u32>()->default_value(std::to_string(Defaults::TRANSPARENT_THRESHOLD)))
        //
        ("p,threads",
         "Number of threads to use for processing, 0 means auto-detect.",
         cxxopts::value<u32>()->default_value(std::to_string(Defaults::THREAD_COUNT)))
        //
        ("h,help", "Show help message");

    options.positional_help("<image> <file>").show_positional_help();
    options.parse_positional({"image", "file"});

    try {
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return std::nullopt;
        }

        if (!result.count("image") || !result.count("file")) {
            throw OptionInvalidException("'image' and 'file' arguments are required.");
        }

        EncodeOptions gifOptions;
        gifOptions.imageFile            = result["image"].as<string>();
        gifOptions.encyptFile           = result["file"].as<string>();
        gifOptions.outputFile           = result["output"].as<string>();
        gifOptions.markText             = result["mark_text"].as<string>();
        gifOptions.disableDither        = result.count("no_dither");
        gifOptions.transparency         = result.count("transparency");
        gifOptions.grayscale            = result.count("grayscale");
        gifOptions.enableLocalPalette   = result.count("local_palette");
        gifOptions.numColors            = result["colors"].as<u32>();
        gifOptions.transparentThreshold = result["threshold"].as<u32>();
        gifOptions.threadCount          = result["threads"].as<u32>();

        if (gifOptions.threadCount == 0) {
            gifOptions.threadCount = getThreadCount();
        }

        gifOptions.checkValid();

        return gifOptions;
    } catch (const cxxopts::exceptions::parsing& e) {
        GeneralLogger::error("Error parsing command line arguments: " + string(e.what()));
    } catch (const OptionInvalidException& e) {
        GeneralLogger::error("Invalid argument: " + string(e.what()));
    } catch (const std::exception& e) {
        GeneralLogger::error("Unexpected error: " + string(e.what()));
    } catch (...) {
        GeneralLogger::error("Unexpected error.");
    }
    std::cout << options.help() << std::endl;
    return std::nullopt;
}

void
EncodeOptions::checkValid() const {
    if (numColors < Limits::MIN_NUM_COLORS || numColors > Limits::MAX_NUM_COLORS) {
        throw OptionInvalidException("Number of colors must be between " + std::to_string(Limits::MIN_NUM_COLORS) +
                                     " and " + std::to_string(Limits::MAX_NUM_COLORS));
    }
    if (!transparency && transparentThreshold > 0) {
        throw OptionInvalidException("Transparent threshold must be 0 when transparency is disabled");
    }
    if (transparentThreshold > 255) {
        throw OptionInvalidException("Transparent threshold must be between 0 and 255");
    }
}