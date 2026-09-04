#include "xwa_remaster/xwau_material_asset.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t kXwauMaterialMaxBytes = 4u * 1024u * 1024u;

static void xwau_material_asset_error(char* error, size_t error_size,
                                      const char* message) {
    if (error && error_size) {
        snprintf(error, error_size, "%s",
                 message ? message : "XWAU material asset error");
    }
}

XwaXwauMaterialAssetResult XwaXwauMaterial_LoadAsset(
    AeronVfs* vfs,
    const char* basename,
    XwaXwauMaterialFile* out,
    char* error,
    size_t error_size) {
    if (error && error_size) {
        error[0] = '\0';
    }

    if (!out) {
        xwau_material_asset_error(
            error, error_size, "invalid XWAU material asset output");
        return XWA_XWAU_MATERIAL_ASSET_ERROR;
    }
    memset(out, 0, sizeof *out);

    if (!vfs || !basename || !basename[0]) {
        xwau_material_asset_error(
            error, error_size, "invalid XWAU material asset arguments");
        return XWA_XWAU_MATERIAL_ASSET_ERROR;
    }

    char path[512];
    const int path_length =
        snprintf(path, sizeof path, "Materials/%s.mat", basename);
    if (path_length < 0 || (size_t)path_length >= sizeof path) {
        xwau_material_asset_error(
            error, error_size, "XWAU material asset path is too long");
        return XWA_XWAU_MATERIAL_ASSET_ERROR;
    }

    if (!AeronVfs_Exists(vfs, AERON_VFS_ROOT_ASSET, path)) {
        return XWA_XWAU_MATERIAL_ASSET_MISSING;
    }

    uint8_t* bytes = NULL;
    size_t size = 0;
    if (!AeronVfs_ReadAll(vfs, AERON_VFS_ROOT_ASSET, path,
                          kXwauMaterialMaxBytes, &bytes, &size)) {
        xwau_material_asset_error(
            error, error_size, "XWAU material asset exists but is unreadable");
        return XWA_XWAU_MATERIAL_ASSET_ERROR;
    }

    const int parsed =
        XwaXwauMaterial_ParseText((const char*)bytes, size, out,
                                  error, error_size);
    free(bytes);

    if (!parsed) {
        return XWA_XWAU_MATERIAL_ASSET_ERROR;
    }

    return XWA_XWAU_MATERIAL_ASSET_LOADED;
}
