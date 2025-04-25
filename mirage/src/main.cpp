#include "gif_mirage.h"
#include "gif_options.h"
#include "imsq.h"

#define CLI_MAIN
#ifdef MOCK_COMMAND_LINE
#define CLI_MAIN_MOCK
#endif  // MOCK_COMMAND_LINE
#include "cli_utils.h"

const std::vector<std::string> g_mockArgs{
    "../../images/气气.gif",
    "../../images/马达.gif",
    "-o",
    "../../images/mirage-output.gif",
};

int
main(int argc, char** argv) {
    if (!GIFImage::ImageSequence::initDecoder(argv[0])) {
        return 1;
    }
    auto options = GIFMirage::Options::parseArgs(argc, argv);
    if (!options) {
        return 1;
    }
    if (!GIFMirage::gifMirageEncode(*options)) {
        return 1;
    }
    return 0;
}