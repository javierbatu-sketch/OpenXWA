#ifndef XWA_REMASTER_XWAU_MATERIAL_ASSET_H
#define XWA_REMASTER_XWAU_MATERIAL_ASSET_H

#include <stddef.h>

#include "aeron/vfs.h"
#include "xwa_remaster/xwau_material.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum XwaXwauMaterialAssetResult {
    XWA_XWAU_MATERIAL_ASSET_ERROR = -1,
    XWA_XWAU_MATERIAL_ASSET_MISSING = 0,
    XWA_XWAU_MATERIAL_ASSET_LOADED = 1
} XwaXwauMaterialAssetResult;

XwaXwauMaterialAssetResult XwaXwauMaterial_LoadAsset(
    AeronVfs* vfs,
    const char* basename,
    XwaXwauMaterialFile* out,
    char* error,
    size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
