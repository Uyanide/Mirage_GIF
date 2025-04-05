#include <iostream>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// 用于颜色量化的函数
cv::Mat
quantizeImage(const cv::Mat& image, int numColors, bool dither) {
    // 确保图像是RGB格式(OpenCV使用BGR)
    cv::Mat rgbImage;
    if (image.channels() == 4) {
        cv::cvtColor(image, rgbImage, cv::COLOR_BGRA2BGR);
    } else if (image.channels() == 3) {
        rgbImage = image.clone();
    } else {
        cv::cvtColor(image, rgbImage, cv::COLOR_GRAY2BGR);
    }

    // 将图像转换为一维数组
    cv::Mat pixels = rgbImage.reshape(3, rgbImage.rows * rgbImage.cols);
    cv::Mat pixels32f;
    pixels.convertTo(pixels32f, CV_32F);

    // 执行K-means聚类进行颜色量化
    std::cout << "Performing K-means clustering..." << std::endl;
    cv::Mat labels, centers;
    cv::TermCriteria criteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 10, 1.0);
    cv::kmeans(pixels32f, numColors, labels, criteria, 3, cv::KMEANS_PP_CENTERS, centers);
    std::cout << "K-means clustering completed." << std::endl;

    // 创建量化后的图像
    cv::Mat result(rgbImage.size(), rgbImage.type());
    cv::Mat resultPixels = result.reshape(3, result.rows * result.cols);

    // 基本颜色量化
    std::vector<cv::Vec3b> colorPalette(numColors);
    for (int i = 0; i < numColors; i++) {
        colorPalette[i] = cv::Vec3b(cv::saturate_cast<uchar>(centers.at<float>(i, 0)),
                                    cv::saturate_cast<uchar>(centers.at<float>(i, 1)),
                                    cv::saturate_cast<uchar>(centers.at<float>(i, 2)));
    }

    // 应用颜色映射
    for (int i = 0; i < pixels.rows; i++) {
        int cluster_idx               = labels.at<int>(i, 0);
        resultPixels.at<cv::Vec3b>(i) = colorPalette[cluster_idx];
    }

    // 如果启用抖动，应用Floyd-Steinberg抖动算法
    if (dither) {
        cv::Mat dithered   = rgbImage.clone();
        cv::Mat quantError = cv::Mat::zeros(rgbImage.size(), CV_32FC3);

        for (int y = 0; y < rgbImage.rows; y++) {
            for (int x = 0; x < rgbImage.cols; x++) {
                // 获取原始像素值（加上之前传播的误差）
                cv::Vec3f oldPixel = rgbImage.at<cv::Vec3b>(y, x);
                if (y > 0 || x > 0) {
                    oldPixel += quantError.at<cv::Vec3f>(y, x);
                }

                // 找到最近的颜色
                float minDist = FLT_MAX;
                int bestIndex = 0;
                for (int i = 0; i < numColors; i++) {
                    cv::Vec3f centerColor(centers.at<float>(i, 0), centers.at<float>(i, 1), centers.at<float>(i, 2));

                    float dist = cv::norm(oldPixel - centerColor);
                    if (dist < minDist) {
                        minDist   = dist;
                        bestIndex = i;
                    }
                }

                // 应用量化颜色
                cv::Vec3b newPixel           = colorPalette[bestIndex];
                dithered.at<cv::Vec3b>(y, x) = newPixel;

                // 计算量化误差
                cv::Vec3f error = oldPixel - cv::Vec3f(newPixel);

                // 分散误差到相邻像素 (Floyd-Steinberg算法)
                if (x + 1 < rgbImage.cols) quantError.at<cv::Vec3f>(y, x + 1) += error * 7.0f / 16.0f;
                if (y + 1 < rgbImage.rows) {
                    if (x > 0) quantError.at<cv::Vec3f>(y + 1, x - 1) += error * 3.0f / 16.0f;
                    quantError.at<cv::Vec3f>(y + 1, x) += error * 5.0f / 16.0f;
                    if (x + 1 < rgbImage.cols) quantError.at<cv::Vec3f>(y + 1, x + 1) += error * 1.0f / 16.0f;
                }
            }
        }

        result = dithered;
    }

    // 如果需要Alpha通道，添加回来
    if (image.channels() == 4) {
        cv::Mat rgba;
        cv::cvtColor(result, rgba, cv::COLOR_BGR2BGRA);
        // 复制Alpha通道
        std::vector<cv::Mat> channels;
        std::vector<cv::Mat> originalChannels;
        cv::split(rgba, channels);
        cv::split(image, originalChannels);
        channels[3] = originalChannels[3];  // 复制原图的Alpha通道
        cv::merge(channels, rgba);
        return rgba;
    }

    return result;
}

int
main(int argc, char* argv[]) {
    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <input_image> <output_image> <num_colors> <dither_flag>" << std::endl;
        return 1;
    }

    const char* input_file  = argv[1];
    const char* output_file = argv[2];
    int num_colors          = atoi(argv[3]);
    int ditherFlag          = atoi(argv[4]);  // 是否使用抖动

    // 读取输入图像
    cv::Mat image = cv::imread(input_file, cv::IMREAD_UNCHANGED);
    if (image.empty()) {
        std::cerr << "Error loading image: " << input_file << std::endl;
        return 1;
    }

    // 执行颜色量化
    cv::Mat quantized = quantizeImage(image, num_colors, ditherFlag != 0);

    // 保存结果
    std::cout << "Saving output image to " << output_file << std::endl;
    std::vector<int> pngParams = {cv::IMWRITE_PNG_COMPRESSION, 9};
    bool success               = cv::imwrite(output_file, quantized, pngParams);

    if (!success) {
        std::cerr << "Failed to write output image" << std::endl;
        return 1;
    }

    std::cout << "Successfully quantized image to " << num_colors << " colors" << std::endl;
    return 0;
}