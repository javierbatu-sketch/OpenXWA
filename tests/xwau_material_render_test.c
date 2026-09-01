#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aeron/vfs.h"
#include "xwa_remaster/xwau_material.h"
#include "xwa_remaster/xwau_material_render.h"

struct AeronVfs {
    int unused;
};

static const char* s_present_path = NULL;
static const char* s_present_text = NULL;
static char s_last_exists_path[128];
static char s_last_read_path[128];

int AeronVfs_Exists(AeronVfs* vfs, AeronVfsRoot root, const char* path) {
    (void)vfs;
    if (root != AERON_VFS_ROOT_ASSET || !path) {
        return 0;
    }
    snprintf(s_last_exists_path, sizeof s_last_exists_path, "%s", path);
    return s_present_path && strcmp(path, s_present_path) == 0;
}

int AeronVfs_ReadAll(AeronVfs* vfs, AeronVfsRoot root, const char* path,
                     size_t max_size, uint8_t** out_data, size_t* out_size) {
    (void)vfs;
    (void)max_size;

    if (!out_data || !out_size || root != AERON_VFS_ROOT_ASSET || !path) {
        return 0;
    }

    snprintf(s_last_read_path, sizeof s_last_read_path, "%s", path);

    if (!s_present_path || !s_present_text ||
        strcmp(path, s_present_path) != 0) {
        return 0;
    }

    const size_t size = strlen(s_present_text);
    uint8_t* copy = (uint8_t*)malloc(size);
    if (!copy) {
        return 0;
    }
    memcpy(copy, s_present_text, size);
    *out_data = copy;
    *out_size = size;
    return 1;
}

static void reset_fixture(const char* path, const char* text) {
    s_present_path = path;
    s_present_text = text;
    s_last_exists_path[0] = '\0';
    s_last_read_path[0] = '\0';
}

static int nearf(float actual, float expected) {
    return fabsf(actual - expected) <= 0.0001f;
}

static int require_config(const XwaXwauMaterialRenderConfig* config,
                          float specular_intensity,
                          float glossiness,
                          float lightness_boost,
                          float saturation_boost,
                          const char* label) {
    if (!nearf(config->specular_intensity, specular_intensity) ||
        !nearf(config->glossiness, glossiness) ||
        !nearf(config->lightness_boost, lightness_boost) ||
        !nearf(config->saturation_boost, saturation_boost)) {
        fprintf(stderr,
                "FAIL %s config got spec=%g gloss=%g light=%g saturation=%g\n",
                label,
                (double)config->specular_intensity,
                (double)config->glossiness,
                (double)config->lightness_boost,
                (double)config->saturation_boost);
        return 0;
    }
    return 1;
}

static int test_config_defaults_and_overlay(void) {
    static const char text[] =
        "; XWAU renderer globals\n"
        "specular_intensity = 0.75\n"
        "glossiness = 256.0\n"
        "lightness_boost = 9.5\n"
        "saturation_boost = 1.25\n"
        "specular_bloom_intensity = 99.0\n"
        "unknown_future_key = 1234\n";

    XwaXwauMaterialRenderConfig config;
    char error[256] = {0};

    XwaXwauMaterialRenderConfig_InitDefaults(&config);
    if (!require_config(&config, 0.5f, 128.0f, 8.0f, 1.0f,
                        "reference defaults")) {
        return 0;
    }

    if (!XwaXwauMaterialRenderConfig_ParseText(
            text, sizeof text - 1u, &config, error, sizeof error)) {
        fprintf(stderr, "FAIL parse valid SSAO.cfg: %s\n", error);
        return 0;
    }

    return require_config(&config, 0.75f, 256.0f, 9.5f, 1.25f,
                          "authored overlay");
}

static int test_config_missing_keys_keep_fallbacks(void) {
    static const char text[] =
        "glossiness = 64\n"
        "unrelated = nope\n";

    XwaXwauMaterialRenderConfig config;
    char error[256] = {0};

    XwaXwauMaterialRenderConfig_InitDefaults(&config);
    if (!XwaXwauMaterialRenderConfig_ParseText(
            text, sizeof text - 1u, &config, error, sizeof error)) {
        fprintf(stderr, "FAIL partial SSAO.cfg: %s\n", error);
        return 0;
    }

    return require_config(&config, 0.5f, 64.0f, 8.0f, 1.0f,
                          "partial fallback");
}

static int test_supported_bad_config_values_are_errors(void) {
    static const char malformed[] =
        "specular_intensity = definitely-not-a-number\n";
    static const char nonfinite[] =
        "glossiness = nan\n";

    XwaXwauMaterialRenderConfig config;
    char error[256] = {0};

    XwaXwauMaterialRenderConfig_InitDefaults(&config);
    if (XwaXwauMaterialRenderConfig_ParseText(
            malformed, sizeof malformed - 1u,
            &config, error, sizeof error)) {
        fprintf(stderr, "FAIL malformed supported key was accepted\n");
        return 0;
    }
    if (error[0] == '\0') {
        fprintf(stderr, "FAIL malformed supported key had no error\n");
        return 0;
    }

    error[0] = '\0';
    XwaXwauMaterialRenderConfig_InitDefaults(&config);
    if (XwaXwauMaterialRenderConfig_ParseText(
            nonfinite, sizeof nonfinite - 1u,
            &config, error, sizeof error)) {
        fprintf(stderr, "FAIL non-finite supported key was accepted\n");
        return 0;
    }
    if (error[0] == '\0') {
        fprintf(stderr, "FAIL non-finite supported key had no error\n");
        return 0;
    }

    return 1;
}

static int test_config_asset_root_loading(void) {
    static const char text[] =
        "specular_intensity = 0.625\n"
        "glossiness = 96\n"
        "lightness_boost = 7\n"
        "saturation_boost = 1.5\n";

    AeronVfs vfs = {0};
    XwaXwauMaterialRenderConfig config;
    char error[256] = {0};

    reset_fixture("SSAO.cfg", text);
    const XwaXwauMaterialRenderConfigAssetResult loaded =
        XwaXwauMaterialRenderConfig_LoadAsset(
            &vfs, &config, error, sizeof error);
    if (loaded != XWA_XWAU_MATERIAL_RENDER_CONFIG_LOADED) {
        fprintf(stderr, "FAIL SSAO.cfg load result=%d error=%s\n",
                (int)loaded, error);
        return 0;
    }
    if (strcmp(s_last_exists_path, "SSAO.cfg") != 0 ||
        strcmp(s_last_read_path, "SSAO.cfg") != 0) {
        fprintf(stderr, "FAIL SSAO.cfg asset-root path exists='%s' read='%s'\n",
                s_last_exists_path, s_last_read_path);
        return 0;
    }
    if (!require_config(&config, 0.625f, 96.0f, 7.0f, 1.5f,
                        "asset loaded")) {
        return 0;
    }

    reset_fixture(NULL, NULL);
    error[0] = '\0';
    const XwaXwauMaterialRenderConfigAssetResult missing =
        XwaXwauMaterialRenderConfig_LoadAsset(
            &vfs, &config, error, sizeof error);
    if (missing != XWA_XWAU_MATERIAL_RENDER_CONFIG_FALLBACK) {
        fprintf(stderr, "FAIL missing SSAO.cfg result=%d error=%s\n",
                (int)missing, error);
        return 0;
    }
    if (s_last_read_path[0] != '\0') {
        fprintf(stderr, "FAIL missing SSAO.cfg was read anyway\n");
        return 0;
    }
    if (error[0] != '\0') {
        fprintf(stderr, "FAIL missing SSAO.cfg produced error=%s\n", error);
        return 0;
    }

    return require_config(&config, 0.5f, 128.0f, 8.0f, 1.0f,
                          "missing asset fallback");
}

static int test_reference_translation_and_unclamped_values(void) {
    XwaXwauMaterialResolved authored;
    XwaXwauMaterialRenderConfig config;
    XwaXwauMaterialLegacy legacy;
    char error[256] = {0};

    memset(&authored, 0, sizeof authored);
    authored.has_glossiness = 1;
    authored.glossiness = 0.02f;
    authored.has_intensity = 1;
    authored.intensity = 0.25f;
    authored.has_metallic = 1;
    authored.metallic = 1.75f;
    authored.has_nm_intensity = 1;
    authored.nm_intensity = 2.5f;
    authored.has_specular_val = 1;
    authored.specular_val = 0.35f;
    authored.has_ambient = 1;
    authored.ambient = 1.3f;
    authored.has_shadeless = 1;
    authored.shadeless = 1;

    XwaXwauMaterialRenderConfig_InitDefaults(&config);
    const XwaXwauMaterialRenderResult result =
        XwaXwauMaterialRender_Resolve(
            &authored, &config, &legacy, error, sizeof error);

    if (result != XWA_XWAU_MATERIAL_RENDER_LEGACY) {
        fprintf(stderr, "FAIL legacy translation result=%d error=%s\n",
                (int)result, error);
        return 0;
    }

    if (!nearf(legacy.legacy_specular_exponent, 2.56f) ||
        !nearf(legacy.legacy_specular_intensity, 0.125f) ||
        !nearf(legacy.legacy_specular_color_control, 1.75f) ||
        !nearf(legacy.legacy_specular_value, 0.35f) ||
        !nearf(legacy.legacy_ambient, 1.3f) ||
        !nearf(legacy.normal_scale, 2.5f) ||
        !nearf(legacy.legacy_lightness_boost, 8.0f) ||
        !nearf(legacy.legacy_saturation_boost, 1.0f) ||
        legacy.legacy_shadeless != 1) {
        fprintf(stderr, "FAIL reference legacy translation values\n");
        return 0;
    }

    return 1;
}

static int test_constructor_defaults_seed_loaded_material(void) {
    XwaXwauMaterialResolved authored;
    XwaXwauMaterialRenderConfig config;
    XwaXwauMaterialLegacy legacy;
    char error[256] = {0};

    memset(&authored, 0, sizeof authored);
    XwaXwauMaterialRenderConfig_InitDefaults(&config);

    const XwaXwauMaterialRenderResult result =
        XwaXwauMaterialRender_Resolve(
            &authored, &config, &legacy, error, sizeof error);

    if (result != XWA_XWAU_MATERIAL_RENDER_LEGACY) {
        fprintf(stderr, "FAIL constructor-default translation: %s\n", error);
        return 0;
    }

    if (!nearf(legacy.legacy_specular_exponent, 2.56f) ||
        !nearf(legacy.legacy_specular_intensity, 0.125f) ||
        !nearf(legacy.legacy_specular_color_control, 0.3f) ||
        !nearf(legacy.legacy_specular_value, 0.0f) ||
        !nearf(legacy.legacy_ambient, 0.0f) ||
        !nearf(legacy.normal_scale, 0.5f) ||
        !nearf(legacy.legacy_lightness_boost, 8.0f) ||
        !nearf(legacy.legacy_saturation_boost, 1.0f) ||
        legacy.legacy_shadeless != 0) {
        fprintf(stderr, "FAIL XWAU constructor defaults\n");
        return 0;
    }

    return 1;
}

static int test_missing_material_produces_no_legacy_override(void) {
    XwaXwauMaterialRenderConfig config;
    XwaXwauMaterialLegacy legacy;
    char error[256] = {0};

    memset(&legacy, 0xA5, sizeof legacy);
    XwaXwauMaterialRenderConfig_InitDefaults(&config);

    const XwaXwauMaterialRenderResult result =
        XwaXwauMaterialRender_Resolve(
            NULL, &config, &legacy, error, sizeof error);

    if (result != XWA_XWAU_MATERIAL_RENDER_NONE) {
        fprintf(stderr, "FAIL missing .mat produced result=%d error=%s\n",
                (int)result, error);
        return 0;
    }
    if (error[0] != '\0') {
        fprintf(stderr, "FAIL missing .mat produced error=%s\n", error);
        return 0;
    }

    return 1;
}

int main(void) {
    if (!test_config_defaults_and_overlay()) {
        return 1;
    }
    if (!test_config_missing_keys_keep_fallbacks()) {
        return 1;
    }
    if (!test_supported_bad_config_values_are_errors()) {
        return 1;
    }
    if (!test_config_asset_root_loading()) {
        return 1;
    }
    if (!test_reference_translation_and_unclamped_values()) {
        return 1;
    }
    if (!test_constructor_defaults_seed_loaded_material()) {
        return 1;
    }
    if (!test_missing_material_produces_no_legacy_override()) {
        return 1;
    }

    puts("PASS: XWAU static material render translation contracts");
    return 0;
}
