#include <vector>

#include "def.h"
#include "quantizer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int
main(int argc, char* argv[]) {
    char* inputFile = "../#000000.png";
    if (argc > 1) inputFile = argv[1];
    uint32_t numColors = 15;
    if (argc > 2) numColors = atoi(argv[2]);
    GIFImage::DitherMode ditherMode = GIFImage::DitherMode(0);
    if (argc > 3) ditherMode = GIFImage::DitherMode(atoi(argv[3]));
    bool grayScale = false;
    if (argc > 4) grayScale = argv[4][0] == '1' || argv[4][0] == 't' || argv[4][0] == 'T';

    int width, height, channels;
    unsigned char* img = stbi_load(inputFile, &width, &height, &channels, 0);
    if (img == NULL) {
        printf("Error in loading the image\n");
        exit(1);
    }

    std::vector<PixelBGRA> pixels;
    pixels.reserve(width * height);
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int idx = (i * width + j) * channels;
            pixels.push_back({img[idx + 2], img[idx + 1], img[idx]});
        }
    }
    auto result = GIFImage::quantize(
        pixels,
        width,
        height,
        numColors,
        ditherMode,
        grayScale,
        false,
        0,
        true);
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int idx          = (i * width + j);
            const auto color = result.palette[result.indices[idx]];
            idx *= channels;
            img[idx]     = color.r;
            img[idx + 1] = color.g;
            img[idx + 2] = color.b;
        }
    }
    unsigned char* paletteImg = new unsigned char[numColors * 4 * 100];
    for (int i = 0; i < numColors * 4 * 100; i += 4) {
        const auto& color = i / 4 / 10 % numColors;
        paletteImg[i]     = result.palette[color].r;
        paletteImg[i + 1] = result.palette[color].g;
        paletteImg[i + 2] = result.palette[color].b;
        paletteImg[i + 3] = 255;  // fully opaque
    }

    stbi_write_png("output.png", width, height, channels, img, width * channels);
    stbi_write_png("palette.png", 10 * numColors, 10, 4, paletteImg, 10 * numColors * 4);
    stbi_image_free(img);
}