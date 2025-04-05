#include "gif_mirage.h"

#include <array>
#include <cmath>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

#include "def.h"
#include "gif_encoder.h"
#include "gif_lzw.h"
#include "gif_options.h"
#include "imsq.h"
#include "log.h"

using std::vector;

using IsCoverFunc      = std::function<bool(u32 x, u32 y)>;
using IsCoverFuncsType = std::array<IsCoverFunc, size_t(GIFMirage::MergeMode::PIVOT)>;
static const IsCoverFuncsType IsCoverFuncs{
    [](u32 x, u32 y) { return ((y + x) & 3) < 2; },
    [](u32 x, u32 _) { return (x & 1) == 0; },
    [](u32 _, u32 y) { return (y & 1) == 0; },
    [](u32 x, u32 y) { return ((y / 3 + x) % 3) < 2; },
    [](u32 x, u32 y) { return ((y + x) & 1) == 0; },
};

static const std::vector<PixelBGRA> GCT{makeBGRA(0, 0, 0), makeBGRA(0x80, 0x80, 0x80), makeBGRA(0xff, 0xff, 0xff)};
static constexpr u32 TRANSPARENT_INDEX = 1;
static constexpr u32 MIN_CODE_LENGTH   = 2;

static inline constexpr u32
u32min(const u32 a, const u32 b) {
    return a < b ? a : b;
}

static vector<u32>
getFrameIndices(const u32* delays, const u32 numFrames, const u32 targetDelay, const u32 targetNumFrames) {
    static const auto round = [](const double value) -> u32 { return static_cast<u32>(std::round(value)); };

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

static constexpr double s_bayerMatrix[4][4] = {{0 / 16. * 255, 8 / 16. * 255, 2 / 16. * 255, 10 / 16. * 255},
                                               {12 / 16. * 255, 4 / 16. * 255, 14 / 16. * 255, 6 / 16. * 255},
                                               {3 / 16. * 255, 11 / 16. * 255, 1 / 16. * 255, 9 / 16. * 255},
                                               {15 / 16. * 255, 7 / 16. * 255, 13 / 16. * 255, 5 / 16. * 255}};

static void
orderedDithering(u8* out, const PixelBGRA* data, const u32 width, const u32 height) noexcept {
    const auto argb = data;
    for (u32 y = 0, idx = 0; y < height; y++) {
        for (u32 x = 0; x < width; x++, idx++) {
            const auto v = *(argb + idx);
            const auto l = toGray(v).r;
            out[idx]     = l > s_bayerMatrix[x & 3][y & 3] ? 255 : 0;
        }
    }
}

// static constexpr double s_bayerMatrix[8][8] = {{0 / 64. * 255,
//                                                 32 / 64. * 255,
//                                                 8 / 64. * 255,
//                                                 40 / 64. * 255,
//                                                 2 / 64. * 255,
//                                                 34 / 64. * 255,
//                                                 10 / 64. * 255,
//                                                 42 / 64. * 255},
//                                                {48 / 64. * 255,
//                                                 16 / 64. * 255,
//                                                 56 / 64. * 255,
//                                                 24 / 64. * 255,
//                                                 50 / 64. * 255,
//                                                 18 / 64. * 255,
//                                                 58 / 64. * 255,
//                                                 26 / 64. * 255},
//                                                {12 / 64. * 255,
//                                                 44 / 64. * 255,
//                                                 4 / 64. * 255,
//                                                 36 / 64. * 255,
//                                                 14 / 64. * 255,
//                                                 46 / 64. * 255,
//                                                 6 / 64. * 255,
//                                                 38 / 64. * 255},
//                                                {60 / 64. * 255,
//                                                 28 / 64. * 255,
//                                                 52 / 64. * 255,
//                                                 20 / 64. * 255,
//                                                 62 / 64. * 255,
//                                                 30 / 64. * 255,
//                                                 54 / 64. * 255,
//                                                 22 / 64. * 255},
//                                                {3 / 64. * 255,
//                                                 35 / 64. * 255,
//                                                 11 / 64. * 255,
//                                                 43 / 64. * 255,
//                                                 1 / 64. * 255,
//                                                 33 / 64. * 255,
//                                                 9 / 64. * 255,
//                                                 41 / 64. * 255},
//                                                {51 / 64. * 255,
//                                                 19 / 64. * 255,
//                                                 59 / 64. * 255,
//                                                 27 / 64. * 255,
//                                                 49 / 64. * 255,
//                                                 17 / 64. * 255,
//                                                 57 / 64. * 255,
//                                                 25 / 64. * 255},
//                                                {15 / 64. * 255,
//                                                 47 / 64. * 255,
//                                                 7 / 64. * 255,
//                                                 39 / 64. * 255,
//                                                 13 / 64. * 255,
//                                                 45 / 64. * 255,
//                                                 5 / 64. * 255,
//                                                 37 / 64. * 255},
//                                                {63 / 64. * 255,
//                                                 31 / 64. * 255,
//                                                 55 / 64. * 255,
//                                                 23 / 64. * 255,
//                                                 61 / 64. * 255,
//                                                 29 / 64. * 255,
//                                                 53 / 64. * 255,
//                                                 21 / 64. * 255}};

// static void
// orderedDithering(u8* out, const PixelBGRA* data, const u32 width, const u32 height) noexcept {
//     const auto argb = data;
//     for (u32 y = 0, idx = 0; y < height; y++) {
//         for (u32 x = 0; x < width; x++, idx++) {
//             const auto v = *(argb + idx);
//             const auto l = toGray(v).r;
//             out[idx]     = l > s_bayerMatrix[x & 7][y & 7] ? 255 : 0;
//         }
//     }
// }

bool
GIFMirage::gifMirageEncode(const GIFMirage::Options& args) {
    GeneralLogger::info("Starting GIF mirage encoding...");
    GeneralLogger::info("Output file: " + args.outputFile, GeneralLogger::STEP);
    GeneralLogger::info("Width: " + std::to_string(args.width), GeneralLogger::STEP);
    GeneralLogger::info("Height: " + std::to_string(args.height), GeneralLogger::STEP);
    GeneralLogger::info("Number of frames: " + std::to_string(args.frameCount), GeneralLogger::STEP);
    GeneralLogger::info("Frame duration: " + std::to_string(args.delay), GeneralLogger::STEP);
    GeneralLogger::info("Merge mode: " + std::to_string(args.mergeMode), GeneralLogger::STEP);

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
    u8** innerFramesCache = new u8* [inner->getFrameCount()] { nullptr };  // GrayScale
    std::mutex innerMutex;
    u8** coverFramesCache = new u8* [cover->getFrameCount()] { nullptr };  // GrayScale
    std::mutex coverMutex;
    vector<vector<u8>> outFrames(args.frameCount);  // GrayScale
    std::mutex cntMutex;
    u32 cnt = 0;

    const auto framesPerThread = (args.frameCount + args.threadCount - 1) / args.threadCount;
    GeneralLogger::info(std::string("Thread count: ") + std::to_string(args.threadCount), GeneralLogger::STEP);
    auto threads = vector<std::thread>(args.threadCount);

    auto isCoverFunc = IsCoverFuncs[args.mergeMode % IsCoverFuncs.size()];
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
                            orderedDithering(innerFrame, frameBuffer.data(), args.width, args.height);
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
                            orderedDithering(coverFrame, frameBuffer.data(), args.width, args.height);
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
            u32min((i + 1) * framesPerThread, args.frameCount));
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
            encoder->addFrameCompressed(outFrames[i], args.delay);
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