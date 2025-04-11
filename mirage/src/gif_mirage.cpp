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

using IsCoverFunc = std::function<bool(u32 x, u32 y)>;

static const vector<PixelBGRA> GCT{makeBGRA(0, 0, 0), makeBGRA(0x80, 0x80, 0x80), makeBGRA(0xff, 0xff, 0xff)};
static constexpr u32 TRANSPARENT_INDEX = 1;
static constexpr u32 MIN_CODE_LENGTH   = 2;
static const auto ditherFunc           = ImageSequence::Dither::BayerOrderedDithering<4>::orderedDithering;

static vector<u32>
getFrameIndices(const u32* delays, const u32 numFrames, const u32 targetDelay, const u32 targetNumFrames) {
    static const auto round = [](const double value) -> u32 {
        return static_cast<u32>(std::round(value));
    };

    u32 srcDuration = 0;
    for (u32 i = 0; i < numFrames; ++i) {
        srcDuration += delays[i];
    }
    if (srcDuration == 0) {
        auto ret = vector<u32>(targetNumFrames);
        for (u32 i = 0; i < targetNumFrames; ++i) {
            ret[i] = i % numFrames;
        }
        return ret;
    }
    u32 totalDuration = targetNumFrames * targetDelay;
    u32 loops         = round(totalDuration / static_cast<double>(srcDuration));
    if (loops == 0) loops = 1;
    double eqDelay = double(targetDelay) * srcDuration * loops / totalDuration;
    auto ret       = vector<u32>(targetNumFrames);
    u32 currFrame = 0, currUntil = delays[0];
    double currTime = eqDelay;
    for (u32 i = 0; i < targetNumFrames; ++i) {
        while (currTime > currUntil) {
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

    GeneralLogger::info("Reading images...");
    auto inner = GIFImage::ImageSequence::read(args.innerFile);
    auto cover = GIFImage::ImageSequence::read(args.coverFile);
    if (!inner || !cover) {
        return false;
    }
    const auto innerIndices =
        getFrameIndices(inner->getDelays().data(), inner->getFrameCount(), args.delay, args.frameCount);
    const auto coverIndices =
        getFrameIndices(cover->getDelays().data(), cover->getFrameCount(), args.delay, args.frameCount);

    GeneralLogger::info("Generating frames...");
    u8** innerFramesCache = new u8* [inner->getFrameCount()] {
        nullptr
    };  // GrayScale
    std::mutex innerMutex;
    u8** coverFramesCache = new u8* [cover->getFrameCount()] {
        nullptr
    };  // GrayScale
    std::mutex coverMutex;
    vector<vector<u8>> outFrames(args.frameCount);  // GrayScale
    std::mutex cntMutex;
    u32 cnt = 0;

    const auto framesPerThread = (args.frameCount + args.threadCount - 1) / args.threadCount;
    GeneralLogger::info(std::string("Thread count: ") + std::to_string(args.threadCount), GeneralLogger::STEP);
    auto threads = vector<std::thread>(args.threadCount);

    const IsCoverFunc isCoverFunc = std::bind(
        [](const u32 slope, const u32 width, const bool isRow, const u32 x, const u32 y) {
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

    for (u32 i = 0; i < args.threadCount; ++i) {
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
             &cntMutex](u32 start, u32 end) {
                for (u32 j = start; j < end; ++j) {
                    u8* innerFrame;
                    u8* coverFrame;
                    {
                        std::lock_guard<std::mutex> lock(innerMutex);
                        innerFrame = innerFramesCache[innerIndices[j]];
                        if (!innerFrame) {
                            const auto frameBuffer =
                                inner->getFrameBuffer(innerIndices[j], args.width, args.height, false);
                            if (frameBuffer.empty()) return;
                            innerFrame = new u8[args.width * args.height];
                            ditherFunc(innerFrame, frameBuffer.data(), args.width, args.height);
                            innerFramesCache[innerIndices[j]] = innerFrame;
                        }
                    }
                    {
                        std::lock_guard<std::mutex> lock(coverMutex);
                        coverFrame = coverFramesCache[coverIndices[j]];
                        if (!coverFrame) {
                            const auto frameBuffer =
                                cover->getFrameBuffer(coverIndices[j], args.width, args.height, false);
                            if (frameBuffer.empty()) return;
                            coverFrame = new u8[args.width * args.height];
                            ditherFunc(coverFrame, frameBuffer.data(), args.width, args.height);
                            coverFramesCache[coverIndices[j]] = coverFrame;
                        }
                    }
                    std::array pixels = {
                        innerFrame,
                        coverFrame,
                    };
                    u64 size = args.width * args.height;
                    vector<u8> merged(size);
                    for (u32 x = 0; x < args.width; ++x) {
                        for (u32 y = 0; y < args.height; ++y) {
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

                    vector<u8> outData;
                    bool isFirst              = true;
                    const auto compressedSize = GIFEnc::LZW::compressStream(
                        [&merged, &isFirst]() -> std::span<u8> {
                            if (isFirst) {
                                isFirst = false;
                                return {merged.data(), merged.size()};
                            } else {
                                return {};
                            }
                        },
                        [&outData](const std::span<u8>& data) {
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
            std::min<u32>((i + 1) * framesPerThread, args.frameCount));
    }

    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    for (u32 i = 0; i < inner->getFrameCount(); ++i) {
        delete[] innerFramesCache[i];
    }
    delete[] innerFramesCache;
    for (u32 i = 0; i < cover->getFrameCount(); ++i) {
        delete[] coverFramesCache[i];
    }
    delete[] coverFramesCache;

    GeneralLogger::info("Writing GIF...");

    try {
        const auto encoder = new GIFEnc::GIFEncoder(args.outputFile,
                                                    args.width,
                                                    args.height,
                                                    TRANSPARENT_INDEX,
                                                    MIN_CODE_LENGTH,
                                                    true,
                                                    TRANSPARENT_INDEX,
                                                    0,
                                                    true,
                                                    GCT);

        for (u32 i = 0; i < args.frameCount; ++i) {
            if (outFrames[i].empty()) continue;
            encoder->addFrameCompressed(outFrames[i], args.delay, args.disposalMethod);
        }
        if (!encoder->finish()) {
            GeneralLogger::error("Failed to write GIF file.");
            delete encoder;
            return false;
        }
        GeneralLogger::info("Output file: " + encoder->getFileName());
        return true;
    } catch (const std::exception& e) {
        GeneralLogger::error(std::string("Failed to write GIF file: ") + e.what());
        return false;
    } catch (...) {
        GeneralLogger::error("Failed to write GIF file: unknown error");
        return false;
    }
}