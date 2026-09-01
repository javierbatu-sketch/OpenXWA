#ifndef XWA_REMASTER_XWAU_MATERIAL_RENDER_H
#define XWA_REMASTER_XWAU_MATERIAL_RENDER_H

#include <stddef.h>

#include "aeron/vfs.h"
#include "xwa_remaster/xwau_material.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XwaXwauMaterialRenderConfig {
    float specular_intensity;
    float glossiness;
    float lightness_boost;
    float saturation_boost;
} XwaXwauMaterialRenderConfig;

typedef enum XwaXwauMaterialRenderConfigAssetResult {
    XWA_XWAU_MATERIAL_RENDER_CONFIG_ERROR = -1,
    XWA_XWAU_MATERIAL_RENDER_CONFIG_FALLBACK = 0,
    XWA_XWAU_MATERIAL_RENDER_CONFIG_LOADED = 1
} XwaXwauMaterialRenderConfigAssetResult;

typedef struct XwaXwauMaterialLegacy {
    float legacy_specular_exponent;
    float legacy_specular_intensity;
    float legacy_specular_color_control;
    float legacy_specular_value;
    float legacy_ambient;
    float normal_scale;
    float legacy_lightness_boost;
    float legacy_saturation_boost;
    int legacy_shadeless;
} XwaXwauMaterialLegacy;

typedef enum XwaXwauMaterialRenderResult {
    XWA_XWAU_MATERIAL_RENDER_ERROR = -1,
    XWA_XWAU_MATERIAL_RENDER_NONE = 0,
    XWA_XWAU_MATERIAL_RENDER_LEGACY = 1
} XwaXwauMaterialRenderResult;

void XwaXwauMaterialRenderConfig_InitDefaults(
    XwaXwauMaterialRenderConfig* config);

int XwaXwauMaterialRenderConfig_ParseText(
    const char* text,
    size_t size,
    XwaXwauMaterialRenderConfig* config,
    char* error,
    size_t error_size);

XwaXwauMaterialRenderConfigAssetResult
XwaXwauMaterialRenderConfig_LoadAsset(
    AeronVfs* vfs,
    XwaXwauMaterialRenderConfig* config,
    char* error,
    size_t error_size);

XwaXwauMaterialRenderResult XwaXwauMaterialRender_Resolve(
    const XwaXwauMaterialResolved* authored,
    const XwaXwauMaterialRenderConfig* config,
    XwaXwauMaterialLegacy* out,
    char* error,
    size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
