#ifndef XWA_REMASTER_XWAU_NORMAL_MAP_H
#define XWA_REMASTER_XWAU_NORMAL_MAP_H

#include <stddef.h>
#include <stdint.h>

#include "aeron/vfs.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    XWA_XWAU_NORMAL_MAP_PATH_CAPACITY = 512
};

typedef struct XwaXwauNormalMapReference {
    char dat_path[XWA_XWAU_NORMAL_MAP_PATH_CAPACITY];
    uint16_t group;
    uint16_t sprite_id;
} XwaXwauNormalMapReference;

typedef struct XwaXwauNormalMapImage {
    uint8_t* rgba;
    int width;
    int height;
} XwaXwauNormalMapImage;

int XwaXwauNormalMap_ParseReference(
    const char* text,
    XwaXwauNormalMapReference* out,
    char* error,
    size_t error_size);

int XwaXwauNormalMap_Load(
    AeronVfs* vfs,
    const char* reference,
    XwaXwauNormalMapImage* out,
    char* error,
    size_t error_size);

void XwaXwauNormalMap_Free(XwaXwauNormalMapImage* image);

#ifdef __cplusplus
}
#endif

#endif
