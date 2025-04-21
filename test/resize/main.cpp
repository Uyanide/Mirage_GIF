#include <iostream>

#include "imsq.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int
main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <input_image> <width> <height>" << std::endl;
        return 1;
    }

    const char* inputImagePath = argv[1];
    int targetWidth            = std::stoi(argv[2]);
    int targetHeight           = std::stoi(argv[3]);
    int width, height, channels;
    unsigned char* data = stbi_load(inputImagePath, &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "Error loading image: " << stbi_failure_reason() << std::endl;
        return 1;
    }
    std::cout << "Original image size: " << width << "x" << height << std::endl;
    std::vector<PixelBGRA> buffer(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int index               = (y * width + x) * channels;
            buffer[y * width + x].r = data[index];
            buffer[y * width + x].g = data[index + 1];
            buffer[y * width + x].b = data[index + 2];
            buffer[y * width + x].a = (channels == 4) ? data[index + 3] : 255;
        }
    }

    const auto resizedBuffer = GIFImage::ImageSequence::resizeCover(
        buffer,
        width,
        height,
        targetWidth,
        targetHeight);

    std::cout << "Resized image size: " << targetWidth << "x" << targetHeight << std::endl;
    unsigned char* outputData = new unsigned char[targetWidth * targetHeight * 4];
    for (int y = 0; y < targetHeight; ++y) {
        for (int x = 0; x < targetWidth; ++x) {
            int index                 = (y * targetWidth + x);
            outputData[index * 4]     = resizedBuffer[index].r;
            outputData[index * 4 + 1] = resizedBuffer[index].g;
            outputData[index * 4 + 2] = resizedBuffer[index].b;
            outputData[index * 4 + 3] = resizedBuffer[index].a;
        }
    }
    if (stbi_write_png("output.png", targetWidth, targetHeight, 4, outputData, targetWidth * 4) == 0) {
        std::cerr << "Error writing output image" << std::endl;
        delete[] outputData;
        stbi_image_free(data);
        return 1;
    }
    std::cout << "Output image saved as output.png" << std::endl;
    delete[] outputData;
    stbi_image_free(data);
    return 0;
}