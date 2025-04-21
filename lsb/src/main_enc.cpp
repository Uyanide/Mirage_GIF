#include "gif_lsb.h"
#include "imsq.h"
#include "options.h"

#define CLI_MAIN
#include "cli_utils.h"

int
main(int argc, char** argv) {

#ifdef MOCK_COMMAND_LINE
    auto mocked = CLIUtils::mockCommandLine(
        {argv[0],
         "../../images/气气.gif",
         "../../images/slime.jpg",
         "-o",
         "../../images/enc-output"});

    argc = mocked.argc();
    argv = mocked.argv();
#endif  // MOCK_COMMAND_LINE

    auto options = GIFLsb::EncodeOptions::parseArgs(argc, argv);
    if (!options) {
        return 1;
    }
    if (!GIFImage::ImageSequence::initDecoder(argv[0])) {
        return 1;
    }
    if (!GIFLsb::gifLsbEncode(*options)) {
        return 1;
    }
    return 0;
}
