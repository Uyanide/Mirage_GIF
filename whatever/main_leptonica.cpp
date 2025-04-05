#include <leptonica/allheaders.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#define STB_IMAGE_IMPLEMENTATION
#include <iostream>
#include <string>

#include "stb_image.h"

int
main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <input_image> <output_image> <num_colors>" << std::endl;
        return 1;
    }

    const char* input_file  = argv[1];
    const char* output_file = argv[2];
    int num_colors          = atoi(argv[3]);
    int ditherFlag          = atoi(argv[4]);  // 是否使用抖动

    // 使用stb_image读取图像
    int width, height, channels;
    unsigned char* img_data = stbi_load(input_file, &width, &height, &channels, 4);  // 强制转为RGBA

    if (!img_data) {
        std::cerr << "Error loading image: " << input_file << std::endl;
        return 1;
    }

    // 将stb_image数据转换为PIX格式
    PIX* pixs = pixCreate(width, height, 32);  // 32位RGBA
    if (!pixs) {
        std::cerr << "Failed to create PIX object" << std::endl;
        stbi_image_free(img_data);
        return 1;
    }

    // 填充PIX数据
    for (int y = 0; y < height; y++) {
        uint32_t* data = pixGetData(pixs);
        int32_t wpl    = pixGetWpl(pixs);
        uint32_t* line = data + y * wpl;
        for (int x = 0; x < width; x++) {
            int idx       = (y * width + x) * 4;
            uint32_t rgba = ((uint32_t)img_data[idx] << 24) |      // R
                            ((uint32_t)img_data[idx + 1] << 16) |  // G
                            ((uint32_t)img_data[idx + 2] << 8) |   // B
                            ((uint32_t)img_data[idx + 3]);         // A
            line[x] = rgba;
        }
    }

    // 释放stb_image数据
    stbi_image_free(img_data);

    // 执行中位切割量化
    PIX* pixd = pixMedianCutQuantGeneral(pixs, ditherFlag, 0, num_colors, 0, 0, 0);
    // PIX* pixd = pixMedianCutQuantMixed(
    //     pixs, 2, 2, 0, 0, 0);
    if (!pixd) {
        std::cerr << "Quantization failed" << std::endl;
        pixDestroy(&pixs);
        return 1;
    }

    // 获取颜色表
    PIXCMAP* colormap = pixGetColormap(pixd);
    if (!colormap) {
        std::cerr << "No colormap in quantized image" << std::endl;
        pixDestroy(&pixs);
        pixDestroy(&pixd);
        return 1;
    }

    // 将量化后的图像转换回普通数据格式
    unsigned char* output_data = new unsigned char[width * height * 4];

    for (int y = 0; y < height; y++) {
        // uint32_t* data = pixGetData(pixd);
        // int32_t wpl = pixGetWpl(pixd);
        // uint32_t* line = data + y * wpl;
        for (int x = 0; x < width; x++) {
            // uint32_t index = GET_DATA_BYTE(line, x);  // 获取索引值

            uint32_t index;
            pixGetPixel(pixd, x, y, &index);  // 获取索引值
            // if (index >= num_colors) {
            //     std::cerr << "Index out of bounds: " << index << std::endl;
            //     // 如果索引超出范围，设置为0（黑色）
            //     index = 0;
            // }

            // 从颜色表获取实际的RGB值
            int r, g, b;
            pixcmapGetColor(colormap, index, &r, &g, &b);

            int idx              = (y * width + x) * 4;
            output_data[idx]     = r;    // R
            output_data[idx + 1] = g;    // G
            output_data[idx + 2] = b;    // B
            output_data[idx + 3] = 255;  // A (通常量化后的图像不保留透明度，设为255)
        }
    }

    // 保存结果
    int success = stbi_write_png(output_file, width, height, 4, output_data, width * 4);

    // 清理资源
    delete[] output_data;
    pixDestroy(&pixs);
    pixDestroy(&pixd);

    if (!success) {
        std::cerr << "Failed to write output image" << std::endl;
        return 1;
    }

    std::cout << "Successfully quantized image to " << num_colors << " colors" << std::endl;
    return 0;
}