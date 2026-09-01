
#include "xwa_remaster/xwau_opt_materials.h"

#include "xwa_remaster/xwau_material_asset.h"
#include "xwa_remaster/xwau_material_render.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int xwau_opt_material_fail(
    char* error,
    size_t error_size,
    const char* message) {

    if (error && error_size && error[0] == '\0') {
        snprintf(error, error_size, "%s",
                 message ? message : "XWAU OPT material integration error");
    }

    return 0;
}

static int xwau_opt_material_ascii_equal(
    const char* left,
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

static int xwau_opt_material_name_exists(
    const XwaXwauOptMaterialState* state,
    const char* name) {

    if (!state || !name) {
        return 0;
    }

    for (size_t i = 1; i < state->override_count; ++i) {
        const char* existing = state->overrides[i].texture_name;
        if (existing && xwau_opt_material_ascii_equal(existing, name)) {
            return 1;
        }
    }

    return 0;
}

static int xwau_opt_material_build_override(
    AeronVfs* vfs,
    const char* texture_name,
    const XwaXwauMaterialResolved* authored,
    const XwaXwauMaterialRenderConfig* config,
    AeronOptMaterialOverride* out_override,
    XwaXwauNormalMapImage* out_image,
    char* error,
    size_t error_size) {

    XwaXwauMaterialLegacy legacy;

    if (!out_override || !out_image) {
        return xwau_opt_material_fail(
            error, error_size, "invalid XWAU OPT material override output");
    }

    memset(out_override, 0, sizeof *out_override);
    memset(out_image, 0, sizeof *out_image);

    const XwaXwauMaterialRenderResult render_result =
        XwaXwauMaterialRender_Resolve(
            authored, config, &legacy, error, error_size);

    if (render_result != XWA_XWAU_MATERIAL_RENDER_LEGACY) {
        return xwau_opt_material_fail(
            error, error_size, "failed to translate XWAU material shading");
    }

    out_override->texture_name = texture_name;
    out_override->flags =
        AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SPECULAR_EXPONENT |
        AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SPECULAR_INTENSITY |
        AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SPECULAR_COLOR_CONTROL |
        AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SPECULAR_VALUE |
        AERON_OPT_MATERIAL_OVERRIDE_LEGACY_AMBIENT |
        AERON_OPT_MATERIAL_OVERRIDE_NORMAL_SCALE |
        AERON_OPT_MATERIAL_OVERRIDE_LEGACY_LIGHTNESS_BOOST |
        AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SATURATION_BOOST |
        AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SHADELESS;

    out_override->legacy_specular_exponent =
        legacy.legacy_specular_exponent;
    out_override->legacy_specular_intensity =
        legacy.legacy_specular_intensity;
    out_override->legacy_specular_color_control =
        legacy.legacy_specular_color_control;
    out_override->legacy_specular_value =
        legacy.legacy_specular_value;
    out_override->legacy_ambient =
        legacy.legacy_ambient;
    out_override->normal_scale =
        legacy.normal_scale;
    out_override->legacy_lightness_boost =
        legacy.legacy_lightness_boost;
    out_override->legacy_saturation_boost =
        legacy.legacy_saturation_boost;
    out_override->legacy_shadeless =
        legacy.legacy_shadeless != 0;

    if (authored && authored->has_normal_map) {
        if (!XwaXwauNormalMap_Load(
                vfs,
                authored->normal_map,
                out_image,
                error,
                error_size)) {
            return xwau_opt_material_fail(
                error, error_size, "failed to resolve XWAU authored NormalMap");
        }

        if (!out_image->rgba ||
            out_image->width <= 0 ||
            out_image->height <= 0) {
            XwaXwauNormalMap_Free(out_image);
            return xwau_opt_material_fail(
                error, error_size, "invalid XWAU authored NormalMap image");
        }

        out_override->flags |=
            AERON_OPT_MATERIAL_OVERRIDE_NORMAL_IMAGE;
        out_override->normal_image.rgba8 = out_image->rgba;
        out_override->normal_image.width =
            (uint32_t)out_image->width;
        out_override->normal_image.height =
            (uint32_t)out_image->height;
    }

    return 1;
}

void XwaXwauOptMaterial_Free(XwaXwauOptMaterialState* state) {
    if (!state) {
        return;
    }

    if (state->normal_images) {
        for (size_t i = 0; i < state->override_count; ++i) {
            if (state->normal_images[i].rgba) {
                XwaXwauNormalMap_Free(&state->normal_images[i]);
            }
        }
    }

    free(state->normal_images);
    free(state->overrides);

    if (state->material_loaded) {
        XwaXwauMaterial_Free(&state->material);
    }

    memset(state, 0, sizeof *state);
}

int XwaXwauOptMaterial_Prepare(
    AeronVfs* vfs,
    const char* basename,
    XwaXwauOptMaterialState* state,
    char* error,
    size_t error_size) {

    if (error && error_size) {
        error[0] = '\0';
    }

    if (!vfs || !basename || !basename[0] || !state) {
        return xwau_opt_material_fail(
            error, error_size, "invalid XWAU OPT material arguments");
    }

    memset(state, 0, sizeof *state);

    const XwaXwauMaterialAssetResult asset_result =
        XwaXwauMaterial_LoadAsset(
            vfs, basename, &state->material, error, error_size);

    if (asset_result == XWA_XWAU_MATERIAL_ASSET_ERROR) {
        return xwau_opt_material_fail(
            error, error_size, "failed to load authored XWAU material");
    }

    if (asset_result == XWA_XWAU_MATERIAL_ASSET_MISSING) {
        return 1;
    }

    state->material_loaded = 1;

    XwaXwauMaterialRenderConfig config;
    if (XwaXwauMaterialRenderConfig_LoadAsset(
            vfs, &config, error, error_size) ==
        XWA_XWAU_MATERIAL_RENDER_CONFIG_ERROR) {
        XwaXwauOptMaterial_Free(state);
        return xwau_opt_material_fail(
            error, error_size, "failed to load XWAU material render config");
    }

    size_t capacity = 1;
    for (size_t section_index = 0;
         section_index < state->material.section_count;
         ++section_index) {
        const size_t names =
            state->material.sections[section_index].name_count;
        if (names > SIZE_MAX - capacity) {
            XwaXwauOptMaterial_Free(state);
            return xwau_opt_material_fail(
                error, error_size, "too many XWAU material section names");
        }
        capacity += names;
    }

    state->overrides = (AeronOptMaterialOverride*)calloc(
        capacity, sizeof *state->overrides);
    state->normal_images = (XwaXwauNormalMapImage*)calloc(
        capacity, sizeof *state->normal_images);

    if (!state->overrides || !state->normal_images) {
        XwaXwauOptMaterial_Free(state);
        return xwau_opt_material_fail(
            error, error_size, "out of memory building XWAU material overrides");
    }

    state->override_capacity = capacity;

    if (!xwau_opt_material_build_override(
            vfs,
            NULL,
            &state->material.defaults,
            &config,
            &state->overrides[0],
            &state->normal_images[0],
            error,
            error_size)) {
        XwaXwauOptMaterial_Free(state);
        return 0;
    }
    state->override_count = 1;

    for (size_t section_index = 0;
         section_index < state->material.section_count;
         ++section_index) {

        const XwaXwauMaterialSection* section =
            &state->material.sections[section_index];

        for (size_t name_index = 0;
             name_index < section->name_count;
             ++name_index) {

            const char* name = section->names[name_index];

            if (!name || !name[0] ||
                xwau_opt_material_name_exists(state, name)) {
                continue;
            }

            XwaXwauMaterialResolved resolved;
            if (!XwaXwauMaterial_Resolve(
                    &state->material,
                    name,
                    &resolved,
                    error,
                    error_size)) {
                XwaXwauOptMaterial_Free(state);
                return xwau_opt_material_fail(
                    error, error_size, "failed to resolve XWAU material section");
            }

            const size_t index = state->override_count;
            if (index >= state->override_capacity) {
                XwaXwauOptMaterial_Free(state);
                return xwau_opt_material_fail(
                    error, error_size, "XWAU material override capacity mismatch");
            }

            if (!xwau_opt_material_build_override(
                    vfs,
                    name,
                    &resolved,
                    &config,
                    &state->overrides[index],
                    &state->normal_images[index],
                    error,
                    error_size)) {
                XwaXwauOptMaterial_Free(state);
                return 0;
            }

            state->override_count++;
        }
    }

    return 1;
}
