#include "gif_lsb.h"
#include "imsq_stream.h"
#include "options.h"

#define CLI_MAIN
#ifdef MOCK_COMMAND_LINE
#define CLI_MAIN_MOCK
#endif  // MOCK_COMMAND_LINE
#include "cli_utils.h"

const std::vector<std::string> g_mockArgs{
    "../../images/enc-output.gif",
    "-o",
    "dec-output",
    "-d",
    "../../images",
};

int
main(int argc, char** argv) {
    if (!GIFImage::ImageSequenceStream::initDecoder(argv[0])) {
        return 1;
    }
    auto options = GIFLsb::DecodeOptions::parseArgs(argc, argv);
    if (!options) {
        return 1;
    }
    if (!GIFLsb::gifLsbDecode(*options)) {
        return 1;
    }
    return 0;
}