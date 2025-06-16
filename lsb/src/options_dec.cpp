#include <optional>
#include <string>

#include "cxxopts.hpp"
#include "file_utils.h"
#include "imsq_stream.h"
#include "log.h"
#include "options.h"

using std::string;

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

static string
genTempName() {
    static const string prefix = "GIFLsb_";
    static const string suffix = ".tmp";
    return prefix + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + suffix;
}

std::optional<GIFLsb::DecodeOptions>
GIFLsb::DecodeOptions::parseArgs(int argc, char** argv) noexcept {
    cxxopts::Options options("GIFLsb", "GIF LSB Decoder");

    options.add_options()
        //
        ("image", "Image to decrypt", cxxopts::value<string>())
        //
        ("o,name",
         "Output filename. If not given, the output file will be saved as filename decrypted from the image.",
         cxxopts::value<string>())
        //
        ("d,directory",
         "Output directory. If not given, the output file will be saved in the current directory.",
         cxxopts::value<string>())
        //
        ("h,help", "Show help message");

    options.positional_help("<image>");
    options.parse_positional({"image"});
    try {
        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            return std::nullopt;
        }

        if (!result.count("image")) {
            throw OptionInvalidException("'image' argument is required.");
        }

        DecodeOptions gifOptions;
        gifOptions.imagePath       = result["image"].as<string>();
        gifOptions.image           = GIFImage::ImageSequenceStream::read(gifOptions.imagePath);
        gifOptions.outputName      = result.count("name") ? result["name"].as<string>() : "";
        gifOptions.outputDirectory = result.count("directory") ? result["directory"].as<string>() : ".";
        gifOptions.tempFileName    = genTempName();
        gifOptions.outputFile      = NaiveIO::FileWriter::create(gifOptions.outputDirectory + '/' + gifOptions.tempFileName);

        const auto ret = gifOptions.ensureValid();
        if (ret) {
            throw OptionInvalidException(std::string(*ret));
        }

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

std::optional<std::string>
GIFLsb::DecodeOptions::ensureValid() {
    if (!image) {
        return "Invalid image file.";
    }
    if (!outputFile) {
        return "Invalid output filename or directory.";
    }
    if (!outputName.empty() && !NaiveIO::isValidFileName(outputName)) {
        return "Invalid output filename: " + outputName;
    }
    if (outputDirectory.back() != '/' && outputDirectory.back() != '\\') {
        outputDirectory.push_back('/');
    }
    return std::nullopt;
}