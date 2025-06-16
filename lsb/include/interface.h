#ifndef GIF_LSB_INFTERFACE_H
#define GIF_LSB_INFTERFACE_H

#include "def.h"

EXTERN_C void EXPORT
gifLsbEncode(
    uint8_t** frames,
    uint32_t* delays,
    uint32_t frameCount,
    uint32_t width,
    uint32_t height,
    uint8_t* data,
    uint32_t dataSize,
    char* markText,
    char* fileName,
    char* outputFilePath,
    char* errorMessage,
    uint32_t errorMessageSize,
    uint32_t colors,
    int32_t grayScale,
    int32_t transparency,
    int32_t transparencyThreshold,
    int32_t localPalette,
    int32_t single);

EXTERN_C void EXPORT
gifLsbDecode(
    uint8_t** frames,
    uint32_t frameCount,
    uint32_t* width,
    uint32_t* height,
    char* outputFilePath);

#endif  // GIF_LSB_INFTERFACE_H