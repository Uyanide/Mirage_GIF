#include "imsq.h"
#include "quantizer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <iostream>
#include <span>
#include <vector>

#include "log.h"
#include "stb_image_write.h"
using namespace std;

std::ostream* GeneralLogger::logStream = &std::cout;

void
GeneralLogger::initLogStream() noexcept {
    if (GeneralLogger::logStream == nullptr) {
        GeneralLogger::logStream = &std::cerr;
    }
}

int
main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <input_image> <output_image> <num_colors>" << std::endl;
        return 1;
    }

    GIFImage::ImageSequence::initDecoder("");

    const char* input_file  = argv[1];
    const char* output_file = argv[2];
    int num_colors          = atoi(argv[3]);

    int width, height, channels;
    unsigned char* img_data = stbi_load(input_file, &width, &height, &channels, 4);  // 强制转为RGBA

    if (!img_data) {
        std::cerr << "Error loading image: " << input_file << std::endl;
        return 1;
    } else {
        std::cout << "Loaded image: " << input_file << " (" << width << "x" << height << ")" << std::endl;
    }

    vector<PixelBGRA> pixels(width * height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx               = (y * width + x) * 4;
            pixels[y * width + x] = makeBGRA(img_data[idx + 2],  // B
                                             img_data[idx + 1],  // G
                                             img_data[idx],      // R
                                             img_data[idx + 3]   // A
            );
        }
    }

    {
        std::span<PixelBGRA> pixels_span(pixels.data(), pixels.size());
        auto ret = GIFImage::ImageSequence::drawText(pixels_span, width, height, "Hello");
        if (!ret) {
            std::cerr << "Error drawing text on image." << std::endl;
            return 1;
        }
    }

    std::cout << "Quantizing image..." << std::endl;
    auto ret = GIFImage::quantize(pixels, width, height, num_colors, GIFImage::DitherOrdered, false, false, 0, false);
    std::cout << "Quantization complete." << std::endl;
    std::cout << "Palette size: " << ret.palette.size() << std::endl;

    // print the palette
    std::cout << "Palette:" << std::endl;
    for (const auto& pixel : ret.palette) {
        std::cout << "BGRA(" << static_cast<int>(pixel.b) << ", " << static_cast<int>(pixel.g) << ", "
                  << static_cast<int>(pixel.r) << ", " << static_cast<int>(pixel.a) << ")" << std::endl;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx           = (y * width + x) * 4;
            const auto& pixel = ret.palette[ret.indices[y * width + x]];
            img_data[idx]     = pixel.r;
            img_data[idx + 1] = pixel.g;
            img_data[idx + 2] = pixel.b;
            img_data[idx + 3] = pixel.a;  // Alpha channel
        }
    }

    stbi_write_png(output_file, width, height, 4, img_data, width * 4);
}