
#ifndef XWA_REMASTER_XWAU_OPT_MATERIALS_H
#define XWA_REMASTER_XWAU_OPT_MATERIALS_H

#include <stddef.h>

#include "aeron/asset/opt_model.h"
#include "aeron/vfs.h"
#include "xwa_remaster/xwau_material.h"
#include "xwa_remaster/xwau_normal_map.h"

typedef struct XwaXwauOptMaterialState {
    XwaXwauMaterialFile material;
    int material_loaded;
    AeronOptMaterialOverride* overrides;
    XwaXwauNormalMapImage* normal_images;
    size_t override_count;
    size_t override_capacity;
} XwaXwauOptMaterialState;

int XwaXwauOptMaterial_Prepare(
    AeronVfs* vfs,
    const char* basename,
    XwaXwauOptMaterialState* state,
    char* error,
    size_t error_size);

void XwaXwauOptMaterial_Free(XwaXwauOptMaterialState* state);

#endif
