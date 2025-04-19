#include "gif_mirage.h"

#include <array>
#include <cmath>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "def.h"
#include "dither.h"
#include "gif_encoder.h"
#include "gif_lzw.h"
#include "gif_options.h"
#include "imsq.h"
#include "log.h"

using std::vector, std::string, std::array;

using IsCoverFunc = std::function<bool(uint32_t x, uint32_t y)>;

static const vector<PixelBGRA> GCT{makeBGRA(0, 0, 0), makeBGRA(0x80, 0x80, 0x80), makeBGRA(0xff, 0xff, 0xff)};
static constexpr uint32_t TRANSPARENT_INDEX = 1;
static constexpr uint32_t MIN_CODE_LENGTH   = 2;
static const auto ditherFunc                = ImageSequence::Dither::BayerOrderedDithering<4>::orderedDithering;

static vector<uint32_t>
getFrameIndices(const vector<uint32_t>& delays, const uint32_t targetDelay, const uint32_t targetNumFrames) {
    static const auto round = [](const double value) -> uint32_t {
        return static_cast<uint32_t>(std::round(value));
    };

    const uint32_t numFrames = delays.size();
    uint32_t srcDuration     = 0;
    for (uint32_t i = 0; i < numFrames; ++i) {
        srcDuration += delays[i];
    }
    if (srcDuration == 0) {
        auto ret = vector<uint32_t>(targetNumFrames);
        for (uint32_t i = 0; i < targetNumFrames; ++i) {
            ret[i] = i % numFrames;
        }
        return ret;
    }
    uint32_t totalDuration = targetNumFrames * targetDelay;
    uint32_t loops         = round(totalDuration / static_cast<double>(srcDuration));
    if (loops == 0) loops = 1;
    double eqDelay = double(srcDuration) * loops / targetNumFrames;
    auto ret       = vector<uint32_t>(targetNumFrames);

    ret[0]             = 0;
    uint32_t currFrame = 0, currUntil = delays[0];
    double currTime = eqDelay;
    for (uint32_t i = 1; i < targetNumFrames; ++i) {
        while (currTime >= currUntil) {
            currFrame = (currFrame + 1) % numFrames;
            currUntil += delays[currFrame];
        }
        ret[i] = currFrame;
        currTime += eqDelay;
    }
    return ret;
}

bool
GIFMirage::gifMirageEncode(const GIFMirage::Options& args) {
    GeneralLogger::info("Starting GIF mirage encoding...");
    GeneralLogger::info("Output file: " + args.outputFile, GeneralLogger::STEP);
    GeneralLogger::info("Width: " + std::to_string(args.width), GeneralLogger::STEP);
    GeneralLogger::info("Height: " + std::to_string(args.height), GeneralLogger::STEP);
    GeneralLogger::info("Number of frames: " + std::to_string(args.frameCount), GeneralLogger::STEP);
    GeneralLogger::info("Frame duration: " + std::to_string(args.delay), GeneralLogger::STEP);
    GeneralLogger::info("Merge mode: " + args.mergeMode.toString(), GeneralLogger::STEP);

    auto& inner = args.innerImage;
    auto& cover = args.coverImage;
    if (!inner || !cover) {
        return false;
    }
    const auto innerIndices =
        getFrameIndices(inner->getDelays(), args.delay, args.frameCount);
    const auto coverIndices =
        getFrameIndices(cover->getDelays(), args.delay, args.frameCount);

    GeneralLogger::info("Generating frames...");
    uint8_t** innerFramesCache = new uint8_t* [inner->getFrameCount()] {
        nullptr
    };  // GrayScale
    std::mutex innerMutex;
    uint8_t** coverFramesCache = new uint8_t* [cover->getFrameCount()] {
        nullptr
    };  // GrayScale
    std::mutex coverMutex;
    vector<vector<uint8_t>> outFrames(args.frameCount);  // GrayScale
    std::mutex cntMutex;
    uint32_t cnt = 0;

    const auto framesPerThread = (args.frameCount + args.threadCount - 1) / args.threadCount;
    GeneralLogger::info(std::string("Thread count: ") + std::to_string(args.threadCount), GeneralLogger::STEP);
    auto threads = vector<std::thread>(args.threadCount);

    const IsCoverFunc isCoverFunc = std::bind(
        [](const uint32_t slope, const uint32_t width, const bool isRow, const uint32_t x, const uint32_t y) {
            if (!slope) {
                return (isRow ? y : x) % (width * 2) < width;
            } else if (isRow) {
                return (y / slope + x) % (width * 2) < width;
            } else {
                return (x / slope + y) % (width * 2) < width;
            }
        },
        args.mergeMode.slope,
        args.mergeMode.width,
        args.mergeMode.isRow,
        std::placeholders::_1,
        std::placeholders::_2);

    for (uint32_t i = 0; i < args.threadCount; ++i) {
        threads[i] = std::thread(
            [&args,
             &innerFramesCache,
             &coverFramesCache,
             &innerMutex,
             &coverMutex,
             &innerIndices,
             &coverIndices,
             &isCoverFunc,
             &inner,
             &cover,
             &outFrames,
             &cnt,
             &cntMutex](uint32_t start, uint32_t end) {
                for (uint32_t j = start; j < end; ++j) {
                    uint8_t* innerFrame;
                    uint8_t* coverFrame;
                    {
                        std::lock_guard<std::mutex> lock(innerMutex);
                        innerFrame = innerFramesCache[innerIndices[j]];
                        if (!innerFrame) {
                            const auto frameBuffer =
                                inner->getFrameBuffer(innerIndices[j], args.width, args.height);
                            if (frameBuffer.empty()) return;
                            innerFrame = new uint8_t[args.width * args.height];
                            ditherFunc(innerFrame, frameBuffer.data(), args.width, args.height);
                            innerFramesCache[innerIndices[j]] = innerFrame;
                        }
                    }
                    {
                        std::lock_guard<std::mutex> lock(coverMutex);
                        coverFrame = coverFramesCache[coverIndices[j]];
                        if (!coverFrame) {
                            const auto frameBuffer =
                                cover->getFrameBuffer(coverIndices[j], args.width, args.height);
                            if (frameBuffer.empty()) return;
                            coverFrame = new uint8_t[args.width * args.height];
                            ditherFunc(coverFrame, frameBuffer.data(), args.width, args.height);
                            coverFramesCache[coverIndices[j]] = coverFrame;
                        }
                    }
                    std::array pixels = {
                        innerFrame,
                        coverFrame,
                    };
                    uint64_t size = args.width * args.height;
                    vector<uint8_t> merged(size);
                    for (uint32_t x = 0; x < args.width; ++x) {
                        for (uint32_t y = 0; y < args.height; ++y) {
                            int i        = y * args.width + x;
                            bool isCover = isCoverFunc(x, y);
                            if ((pixels[isCover][i] > 128) == isCover) {
                                merged[i] = 1;
                            } else if (isCover) {
                                merged[i] = 0;
                            } else {
                                merged[i] = 2;
                            }
                        }
                    }

                    vector<uint8_t> outData;
                    bool isFirst              = true;
                    const auto compressedSize = GIFEnc::LZW::compressStream(
                        [&merged, &isFirst]() -> std::span<uint8_t> {
                            if (isFirst) {
                                isFirst = false;
                                return {merged.data(), merged.size()};
                            } else {
                                return {};
                            }
                        },
                        [&outData](const std::span<uint8_t>& data) {
                            if (data.empty()) return;
                            outData.push_back(data.size());
                            outData.insert(outData.end(), data.begin(), data.end());
                        },
                        nullptr,
                        MIN_CODE_LENGTH,
                        255);
                    if (compressedSize == 0) {
                        GeneralLogger::error("Failed to compress frame data.");
                        return;
                    }
                    outData.push_back(0);
                    outFrames[j] = std::move(outData);
                    {
                        std::lock_guard<std::mutex> lock(cntMutex);
                        if (++cnt % 10 == 0) {
                            GeneralLogger::info(
                                std::to_string(cnt) + " of " + std::to_string(args.frameCount) + " frames processed.",
                                GeneralLogger::STEP);
                        }
                    }
                }
            },
            i * framesPerThread,
            std::min<uint32_t>((i + 1) * framesPerThread, args.frameCount));
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    for (uint32_t i = 0; i < inner->getFrameCount(); ++i) {
        delete[] innerFramesCache[i];
    }
    delete[] innerFramesCache;
    for (uint32_t i = 0; i < cover->getFrameCount(); ++i) {
        delete[] coverFramesCache[i];
    }
    delete[] coverFramesCache;

    GeneralLogger::info("Writing GIF...");

    GIFEnc::GIFEncoder* encoder = nullptr;
    try {
        encoder = new GIFEnc::GIFEncoder(args.outputFile,
                                         args.width,
                                         args.height,
                                         TRANSPARENT_INDEX,
                                         MIN_CODE_LENGTH,
                                         true,
                                         TRANSPARENT_INDEX,
                                         0,
                                         true,
                                         GCT);

        for (uint32_t i = 0; i < args.frameCount; ++i) {
            if (outFrames[i].empty()) continue;
            encoder->addFrameCompressed(outFrames[i], args.delay, args.disposalMethod);
        }
        if (!encoder->finish()) {
            GeneralLogger::error("Failed to write GIF file.");
            delete encoder;
            return false;
        }
        GeneralLogger::info("Output file: " + encoder->getFileName());
        delete encoder;
        return true;
    } catch (const std::exception& e) {
        GeneralLogger::error(std::string("Failed to write GIF file: ") + e.what());
    } catch (...) {
        GeneralLogger::error("Failed to write GIF file: unknown error");
    }
    delete encoder;
    return false;
}