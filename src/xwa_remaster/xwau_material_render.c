#include "xwa_remaster/xwau_material_render.h"

#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t kXwauMaterialRenderConfigMaxBytes = 1024u * 1024u;

static void xwau_material_render_error(char* error, size_t error_size,
                                       const char* message) {
    if (error && error_size) {
        snprintf(error, error_size, "%s",
                 message ? message : "XWAU material render error");
    }
}

static char* xwau_material_render_trim(char* text) {
    while (*text && isspace((unsigned char)*text)) {
        ++text;
    }

    char* end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }

    return text;
}

static int xwau_material_render_ascii_equal(const char* left,
                                            const char* right) {
    if (!left || !right) {
        return 0;
    }

    while (*left && *right) {
        const unsigned char a = (unsigned char)*left++;
        const unsigned char b = (unsigned char)*right++;
        if (tolower(a) != tolower(b)) {
            return 0;
        }
    }

    return *left == '\0' && *right == '\0';
}

static int xwau_material_render_parse_float(const char* value, float* out) {
    if (!value || !out) {
        return 0;
    }

    char* end = NULL;
    const float parsed = strtof(value, &end);
    if (end == value || !isfinite(parsed)) {
        return 0;
    }

    while (*end && isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0') {
        return 0;
    }

    *out = parsed;
    return 1;
}

void XwaXwauMaterialRenderConfig_InitDefaults(
    XwaXwauMaterialRenderConfig* config) {
    if (!config) {
        return;
    }

    config->specular_intensity = 0.5f;
    config->glossiness = 128.0f;
    config->lightness_boost = 8.0f;
    config->saturation_boost = 1.0f;
}

int XwaXwauMaterialRenderConfig_ParseText(
    const char* text,
    size_t size,
    XwaXwauMaterialRenderConfig* config,
    char* error,
    size_t error_size) {
    if (error && error_size) {
        error[0] = '\0';
    }
    if (!text || !config) {
        xwau_material_render_error(
            error, error_size, "invalid XWAU material render config arguments");
        return 0;
    }

    char* copy = (char*)malloc(size + 1u);
    if (!copy) {
        xwau_material_render_error(
            error, error_size, "out of memory parsing XWAU material render config");
        return 0;
    }
    memcpy(copy, text, size);
    copy[size] = '\0';

    char* cursor = copy;
    while (*cursor) {
        char* line = cursor;
        char* newline = strpbrk(cursor, "\r\n");
        if (newline) {
            *newline = '\0';
            cursor = newline + 1;
            while (*cursor == '\r' || *cursor == '\n') {
                ++cursor;
            }
        } else {
            cursor += strlen(cursor);
        }

        line = xwau_material_render_trim(line);
        if (!*line || *line == ';' || *line == '#') {
            continue;
        }

        char* equals = strchr(line, '=');
        if (!equals) {
            continue;
        }

        *equals = '\0';
        char* key = xwau_material_render_trim(line);
        char* value = xwau_material_render_trim(equals + 1);

        char* semicolon = strchr(value, ';');
        char* hash = strchr(value, '#');
        char* comment = NULL;
        if (semicolon && hash) {
            comment = semicolon < hash ? semicolon : hash;
        } else {
            comment = semicolon ? semicolon : hash;
        }
        if (comment) {
            *comment = '\0';
            value = xwau_material_render_trim(value);
        }

        float* destination = NULL;
        if (xwau_material_render_ascii_equal(key, "specular_intensity")) {
            destination = &config->specular_intensity;
        } else if (xwau_material_render_ascii_equal(key, "glossiness")) {
            destination = &config->glossiness;
        } else if (xwau_material_render_ascii_equal(key, "lightness_boost")) {
            destination = &config->lightness_boost;
        } else if (xwau_material_render_ascii_equal(key, "saturation_boost")) {
            destination = &config->saturation_boost;
        } else {
            continue;
        }

        float parsed = 0.0f;
        if (!xwau_material_render_parse_float(value, &parsed)) {
            free(copy);
            xwau_material_render_error(
                error, error_size,
                "invalid XWAU material render config numeric value");
            return 0;
        }
        *destination = parsed;
    }

    free(copy);
    return 1;
}

XwaXwauMaterialRenderConfigAssetResult
XwaXwauMaterialRenderConfig_LoadAsset(
    AeronVfs* vfs,
    XwaXwauMaterialRenderConfig* config,
    char* error,
    size_t error_size) {
    if (error && error_size) {
        error[0] = '\0';
    }
    if (!vfs || !config) {
        xwau_material_render_error(
            error, error_size, "invalid XWAU material render config asset arguments");
        return XWA_XWAU_MATERIAL_RENDER_CONFIG_ERROR;
    }

    XwaXwauMaterialRenderConfig_InitDefaults(config);

    if (!AeronVfs_Exists(vfs, AERON_VFS_ROOT_ASSET, "SSAO.cfg")) {
        return XWA_XWAU_MATERIAL_RENDER_CONFIG_FALLBACK;
    }

    uint8_t* bytes = NULL;
    size_t size = 0;
    if (!AeronVfs_ReadAll(vfs, AERON_VFS_ROOT_ASSET, "SSAO.cfg",
                          kXwauMaterialRenderConfigMaxBytes,
                          &bytes, &size)) {
        xwau_material_render_error(
            error, error_size, "XWAU SSAO.cfg exists but is unreadable");
        return XWA_XWAU_MATERIAL_RENDER_CONFIG_ERROR;
    }

    const int parsed = XwaXwauMaterialRenderConfig_ParseText(
        (const char*)bytes, size, config, error, error_size);
    free(bytes);

    if (!parsed) {
        return XWA_XWAU_MATERIAL_RENDER_CONFIG_ERROR;
    }

    return XWA_XWAU_MATERIAL_RENDER_CONFIG_LOADED;
}

XwaXwauMaterialRenderResult XwaXwauMaterialRender_Resolve(
    const XwaXwauMaterialResolved* authored,
    const XwaXwauMaterialRenderConfig* config,
    XwaXwauMaterialLegacy* out,
    char* error,
    size_t error_size) {
    if (error && error_size) {
        error[0] = '\0';
    }
    if (!out) {
        xwau_material_render_error(
            error, error_size, "invalid XWAU material render output");
        return XWA_XWAU_MATERIAL_RENDER_ERROR;
    }
    memset(out, 0, sizeof *out);

    if (!authored) {
        return XWA_XWAU_MATERIAL_RENDER_NONE;
    }
    if (!config) {
        xwau_material_render_error(
            error, error_size, "invalid XWAU material render config");
        return XWA_XWAU_MATERIAL_RENDER_ERROR;
    }
    if (!isfinite(config->specular_intensity) ||
        !isfinite(config->glossiness) ||
        !isfinite(config->lightness_boost) ||
        !isfinite(config->saturation_boost)) {
        xwau_material_render_error(
            error, error_size, "non-finite XWAU material render config");
        return XWA_XWAU_MATERIAL_RENDER_ERROR;
    }

    float glossiness = 0.02f;
    float intensity = 0.25f;
    float metallic = 0.3f;
    float nm_intensity = 0.5f;
    float specular_val = 0.0f;
    float ambient = 0.0f;
    int shadeless = 0;

    if (authored->has_glossiness) {
        glossiness = authored->glossiness;
    }
    if (authored->has_intensity) {
        intensity = authored->intensity;
    }
    if (authored->has_metallic) {
        metallic = authored->metallic;
    }
    if (authored->has_nm_intensity) {
        nm_intensity = authored->nm_intensity;
    }
    if (authored->has_specular_val) {
        specular_val = authored->specular_val;
    }
    if (authored->has_ambient) {
        ambient = authored->ambient;
    }
    if (authored->has_shadeless) {
        shadeless = authored->shadeless ? 1 : 0;
    }

    if (!isfinite(glossiness) || !isfinite(intensity) ||
        !isfinite(metallic) || !isfinite(nm_intensity) ||
        !isfinite(specular_val) || !isfinite(ambient)) {
        xwau_material_render_error(
            error, error_size, "non-finite XWAU authored material value");
        return XWA_XWAU_MATERIAL_RENDER_ERROR;
    }

    const float exponent_product = glossiness * config->glossiness;
    const float intensity_product = intensity * config->specular_intensity;
    if (!isfinite(exponent_product) || !isfinite(intensity_product)) {
        xwau_material_render_error(
            error, error_size, "XWAU material render translation overflow");
        return XWA_XWAU_MATERIAL_RENDER_ERROR;
    }

    out->legacy_specular_exponent = fmaxf(exponent_product, 0.05f);
    out->legacy_specular_intensity = intensity_product;
    out->legacy_specular_color_control = metallic;
    out->legacy_specular_value = specular_val;
    out->legacy_ambient = ambient;
    out->normal_scale = nm_intensity;
    out->legacy_lightness_boost = config->lightness_boost;
    out->legacy_saturation_boost = config->saturation_boost;
    out->legacy_shadeless = shadeless;

    return XWA_XWAU_MATERIAL_RENDER_LEGACY;
}
