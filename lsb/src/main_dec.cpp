#include "gif_lsb.h"
#include "imsq_stream.h"
#include "options.h"

#define CLI_MAIN
#include "cli_utils.h"

int
main(int argc, char** argv) {

#ifdef MOCK_COMMAND_LINE
    auto mocked = CLIUtils::mockCommandLine({
        argv[0],
        "../../images/enc-output.gif",
        "-o",
        "dec-output",
        "-d",
        "../../images",
    });

    argv = mocked.argv();
    argc = mocked.argc();
#endif  // MOCK_COMMAND_LINE

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