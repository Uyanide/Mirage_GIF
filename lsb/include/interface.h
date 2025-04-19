#ifndef GIFLSB_INTERFACE_H
#define GIFLSB_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include <stdbool.h>
#include <stdint.h>
#endif

int
gifLsbEncode(
    const char* imageFile,
    const char* encryptFile,
    const char* markText,
    bool disableDither,
    bool transparency,
    bool grayscale,
    bool enableLocalPalette,
    bool singleFrame,
    const char* outputFile,
    uint32_t numColors,
    uint32_t transparentThreshold,
    double markRatio);

int
gifLsbDeocode(
    const char* decryptImage,
    const char* outputFile,
    const char* outputDirectory);

#ifdef __cplusplus
}
#endif

#endif  // GIFLSB_INTERFACE_H