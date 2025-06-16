#include <fstream>

#include "interface.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

int
main(int argc, char* argv[]) {
    char* image = "../slime.jpg";
    if (argc > 1) image = argv[1];
    char* file = "../saitama.png";
    if (argc > 2) file = argv[2];

    int width, height, channels;
    unsigned char* img = stbi_load(image, &width, &height, &channels, 0);
    if (img == NULL) {
        printf("Error in loading the image\n");
        exit(1);
    }

    uint8_t* pixels = new uint8_t[width * height * 4];
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            int idx            = (i * width + j) * channels;
            int tarIdx         = (i * width + j) * 4;
            pixels[tarIdx]     = img[idx];
            pixels[tarIdx + 1] = img[idx + 1];
            pixels[tarIdx + 2] = img[idx + 2];
            pixels[tarIdx + 3] = 255;  // Alpha channel
        }
    }

    uint32_t delays         = 0;
    char errorMessage[1024] = {0};

    uint8_t* data;
    size_t dataSize = 0;
    std::ifstream fileStream(file, std::ios::binary | std::ios::ate);
    if (fileStream.is_open()) {
        fileStream.seekg(0, std::ios::end);
        dataSize = fileStream.tellg();
        fileStream.seekg(0, std::ios::beg);
        data = new uint8_t[dataSize];
        fileStream.read(reinterpret_cast<char*>(data), dataSize);
        fileStream.close();
    } else {
        fprintf(stderr, "Error opening file: %s\n", file);
        return 1;
    }

    gifLsbEncode(
        &pixels,
        &delays,
        1,
        width,
        height,
        data,
        dataSize,
        file,
        "output.gif",
        errorMessage,
        sizeof(errorMessage),
        16,
        0,
        0,
        0,
        0,
        0,
        0);
    stbi_image_free(img);
}