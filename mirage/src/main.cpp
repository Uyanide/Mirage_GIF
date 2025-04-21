#include "gif_mirage.h"
#include "gif_options.h"
#include "imsq.h"

#define CLI_MAIN
#include "cli_utils.h"

int
main(int argc, char** argv) {

#ifdef MOCK_COMMAND_LINE
    auto mocked = CLIUtils::mockCommandLine(
        {argv[0],
         "../../images/气气.gif",
         "../../images/马达.gif",
         "-o",
         "../../images/mirage-output.gif",
         "-x",
         "2048",
         "-y",
         "2048",
         "-f",
         "1000"});

    argc = mocked.argc();
    argv = mocked.argv();
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