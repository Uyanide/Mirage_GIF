#include "gif_lsb.h"
#include "imsq.h"
#include "options.h"

#define CLI_MAIN
#ifdef MOCK_COMMAND_LINE
#define CLI_MAIN_MOCK
#endif  // MOCK_COMMAND_LINE
#include "cli_utils.h"

const std::vector<std::string> g_mockArgs{
    "../../images/气气.gif",
    "../../images/slime.jpg",
    "-o",
    "../../images/enc-output",
};

int
main(int argc, char** argv) {
    if (!GIFImage::ImageSequence::initDecoder(argv[0])) {
        return 1;
    }
    auto options = GIFLsb::EncodeOptions::parseArgs(argc, argv);
    if (!options) {
        return 1;
    }
    if (!GIFLsb::gifLsbEncode(*options)) {
        return 1;
    }
    return 0;
}
