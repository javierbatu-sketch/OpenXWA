#include "xwa_remaster/opt_mesh.h"
#include "xwa_remaster/xwau_material_asset.h"
#include "xwa_remaster/xwau_material_render.h"
#include "xwa_remaster/xwau_normal_map.h"

#include "aeron/asset/opt_model.h"
#include "aeron/config_file.h"
#include "aeron/log.h"
#include "aeron/vfs.h"

#include <ctype.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct AeronVfs {
    int unused;
};

struct AeronConfigFile {
    int unused;
};

struct AeronConfigNode {
    AeronConfigNodeType type;
    int id;
};

enum {
    NODE_ROOT = 1,
    NODE_VERSION = 2,
    NODE_MATERIALS = 3
};

static struct AeronConfigFile g_config_file;
static struct AeronConfigNode g_root_node = { AERON_CONFIG_MAP, NODE_ROOT };
static struct AeronConfigNode g_version_node = { AERON_CONFIG_INT, NODE_VERSION };
static struct AeronConfigNode g_materials_node = { AERON_CONFIG_SEQUENCE, NODE_MATERIALS };

static int ascii_equal_ci(const char* a, const char* b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

int AeronConfigFile_LoadYaml(AeronVfs* vfs, AeronVfsRoot root,
                             const char* path, AeronConfigFile** out_config) {
    (void)vfs;
    if (!out_config || root != AERON_VFS_ROOT_RESOURCE ||
        !path || strcmp(path, "remaster/opt_alpha_overrides.yaml") != 0)
        return 0;
    *out_config = &g_config_file;
    return 1;
}

void AeronConfigFile_Destroy(AeronConfigFile* config) {
    (void)config;
}

const AeronConfigNode* AeronConfigFile_Root(const AeronConfigFile* config) {
    return config ? &g_root_node : NULL;
}

AeronConfigNodeType AeronConfigNode_Type(const AeronConfigNode* node) {
    return node ? node->type : AERON_CONFIG_NULL;
}

const AeronConfigNode* AeronConfigNode_MapGet(const AeronConfigNode* node,
                                               const char* key) {
    if (node != &g_root_node || !key) return NULL;
    if (strcmp(key, "version") == 0) return &g_version_node;
    if (strcmp(key, "materials") == 0) return &g_materials_node;
    return NULL;
}

int64_t AeronConfigNode_Int(const AeronConfigNode* node, int64_t fallback) {
    return node == &g_version_node ? 1 : fallback;
}

size_t AeronConfigNode_SequenceCount(const AeronConfigNode* node) {
    return node == &g_materials_node ? 0u : 0u;
}

const AeronConfigNode* AeronConfigNode_SequenceGet(const AeronConfigNode* node,
                                                    size_t index) {
    (void)node;
    (void)index;
    return NULL;
}

const char* AeronConfigNode_String(const AeronConfigNode* node,
                                   const char* fallback) {
    (void)node;
    return fallback;
}

double AeronConfigNode_Float(const AeronConfigNode* node, double fallback) {
    (void)node;
    return fallback;
}

void Aeron_LogVerbose(const char* category, const char* fmt, ...) {
    (void)category;
    (void)fmt;
}

void Aeron_LogInfo(const char* category, const char* fmt, ...) {
    (void)category;
    (void)fmt;
}

void Aeron_LogWarn(const char* category, const char* fmt, ...) {
    (void)category;
    (void)fmt;
}

void Aeron_LogError(const char* category, const char* fmt, ...) {
    (void)category;
    (void)fmt;
}

void Aeron_LogCritical(const char* category, const char* fmt, ...) {
    (void)category;
    (void)fmt;
}

static int g_model_read_calls;
static char g_model_read_path[512];

int AeronVfs_ReadAll(AeronVfs* vfs, AeronVfsRoot root, const char* path,
                     size_t max_size, uint8_t** out_data, size_t* out_size) {
    (void)vfs;
    (void)max_size;

    if (!out_data || !out_size || root != AERON_VFS_ROOT_ASSET || !path)
        return 0;

    if (strncmp(path, "FLIGHTMODELS/", 13) != 0)
        return 0;

    ++g_model_read_calls;
    snprintf(g_model_read_path, sizeof g_model_read_path, "%s", path);

    uint8_t* bytes = (uint8_t*)malloc(4);
    if (!bytes) return 0;
    bytes[0] = 'O';
    bytes[1] = 'P';
    bytes[2] = 'T';
    bytes[3] = 0;
    *out_data = bytes;
    *out_size = 4;
    return 1;
}

int AeronVfs_Exists(AeronVfs* vfs, AeronVfsRoot root, const char* path) {
    (void)vfs;
    (void)root;
    (void)path;
    return 0;
}

typedef enum MaterialMode {
    MATERIAL_MODE_MISSING = 0,
    MATERIAL_MODE_LOADED = 1,
    MATERIAL_MODE_ERROR = 2
} MaterialMode;

static MaterialMode g_material_mode;
static int g_material_load_calls;
static int g_material_free_calls;
static char g_material_basename[128];
static int g_material_resolve_calls;
static char g_material_resolve_names[8][96];
static int g_render_config_calls;
static int g_render_config_error;
static int g_render_resolve_calls;
static int g_normal_load_calls;
static int g_normal_free_calls;
static int g_normal_fail;
static char g_normal_reference[256];
static uint8_t g_normal_pixels[8][4];

static char g_name_hull[] = "Hull";
static char g_name_hull_upper[] = "HULL";
static char g_name_hull_lower[] = "hull";
static char g_name_tex[] = "TEX00007";
static char* g_section0_names[] = { g_name_hull, g_name_hull_upper };
static char* g_section1_names[] = { g_name_tex, g_name_hull_lower };
static XwaXwauMaterialSection g_sections[2];

static void fill_defaults(XwaXwauMaterialResolved* out) {
    memset(out, 0, sizeof *out);
    out->has_glossiness = 1;
    out->glossiness = 0.25f;
    out->has_intensity = 1;
    out->intensity = 0.5f;
    out->has_metallic = 1;
    out->metallic = 1.75f;
    out->has_nm_intensity = 1;
    out->nm_intensity = 2.5f;
    out->has_specular_val = 1;
    out->specular_val = 0.35f;
    out->has_ambient = 1;
    out->ambient = 1.3f;
    out->has_shadeless = 1;
    out->shadeless = 0;
}

XwaXwauMaterialAssetResult XwaXwauMaterial_LoadAsset(
    AeronVfs* vfs,
    const char* basename,
    XwaXwauMaterialFile* out,
    char* error,
    size_t error_size) {

    (void)vfs;
    ++g_material_load_calls;
    snprintf(g_material_basename, sizeof g_material_basename, "%s",
             basename ? basename : "");

    if (error && error_size) error[0] = '\0';
    if (out) memset(out, 0, sizeof *out);

    if (g_material_mode == MATERIAL_MODE_ERROR) {
        if (error && error_size)
            snprintf(error, error_size, "synthetic material asset error");
        return XWA_XWAU_MATERIAL_ASSET_ERROR;
    }

    if (g_material_mode == MATERIAL_MODE_MISSING)
        return XWA_XWAU_MATERIAL_ASSET_MISSING;

    if (!out) return XWA_XWAU_MATERIAL_ASSET_ERROR;

    fill_defaults(&out->defaults);

    memset(g_sections, 0, sizeof g_sections);
    g_sections[0].names = g_section0_names;
    g_sections[0].name_count = 2;
    g_sections[1].names = g_section1_names;
    g_sections[1].name_count = 2;

    out->sections = g_sections;
    out->section_count = 2;
    out->section_capacity = 2;
    return XWA_XWAU_MATERIAL_ASSET_LOADED;
}

void XwaXwauMaterial_Free(XwaXwauMaterialFile* file) {
    ++g_material_free_calls;
    if (file) memset(file, 0, sizeof *file);
}

int XwaXwauMaterial_Resolve(const XwaXwauMaterialFile* file,
                            const char* material_name,
                            XwaXwauMaterialResolved* out,
                            char* error, size_t error_size) {
    (void)file;
    if (error && error_size) error[0] = '\0';
    if (!material_name || !out) return 0;

    if (g_material_resolve_calls < 8) {
        snprintf(g_material_resolve_names[g_material_resolve_calls],
                 sizeof g_material_resolve_names[0], "%s", material_name);
    }
    ++g_material_resolve_calls;

    memset(out, 0, sizeof *out);

    if (ascii_equal_ci(material_name, "Hull")) {
        out->has_glossiness = 1;
        out->glossiness = 0.10f;
        out->has_intensity = 1;
        out->intensity = 0.20f;
        out->has_metallic = 1;
        out->metallic = 0.80f;
        out->has_nm_intensity = 1;
        out->nm_intensity = 1.25f;
        out->has_specular_val = 1;
        out->specular_val = 0.60f;
        out->has_ambient = 1;
        out->ambient = 0.40f;
        out->has_shadeless = 1;
        out->shadeless = 1;
        out->has_normal_map = 1;
        snprintf(out->normal_map, sizeof out->normal_map,
                 "Effects\\Test-Normal.dat-42-7");
        return 1;
    }

    if (ascii_equal_ci(material_name, "TEX00007")) {
        out->has_glossiness = 1;
        out->glossiness = 0.05f;
        out->has_intensity = 1;
        out->intensity = 0.70f;
        out->has_metallic = 1;
        out->metallic = 1.20f;
        out->has_nm_intensity = 1;
        out->nm_intensity = 0.90f;
        out->has_specular_val = 1;
        out->specular_val = 0.20f;
        out->has_ambient = 1;
        out->ambient = 0.60f;
        out->has_shadeless = 1;
        out->shadeless = 0;
        return 1;
    }

    if (error && error_size)
        snprintf(error, error_size, "unexpected synthetic material name");
    return 0;
}

XwaXwauMaterialRenderConfigAssetResult
XwaXwauMaterialRenderConfig_LoadAsset(
    AeronVfs* vfs,
    XwaXwauMaterialRenderConfig* config,
    char* error,
    size_t error_size) {

    (void)vfs;
    ++g_render_config_calls;
    if (error && error_size) error[0] = '\0';

    if (g_render_config_error) {
        if (error && error_size)
            snprintf(error, error_size, "synthetic SSAO config error");
        return XWA_XWAU_MATERIAL_RENDER_CONFIG_ERROR;
    }

    if (!config) return XWA_XWAU_MATERIAL_RENDER_CONFIG_ERROR;
    config->specular_intensity = 0.5f;
    config->glossiness = 128.0f;
    config->lightness_boost = 8.0f;
    config->saturation_boost = 1.0f;
    return XWA_XWAU_MATERIAL_RENDER_CONFIG_LOADED;
}

XwaXwauMaterialRenderResult XwaXwauMaterialRender_Resolve(
    const XwaXwauMaterialResolved* authored,
    const XwaXwauMaterialRenderConfig* config,
    XwaXwauMaterialLegacy* out,
    char* error,
    size_t error_size) {

    ++g_render_resolve_calls;
    if (error && error_size) error[0] = '\0';
    if (!authored || !config || !out) return XWA_XWAU_MATERIAL_RENDER_ERROR;

    memset(out, 0, sizeof *out);
    out->legacy_specular_exponent = authored->glossiness * config->glossiness;
    out->legacy_specular_intensity = authored->intensity * config->specular_intensity;
    out->legacy_specular_color_control = authored->metallic;
    out->legacy_specular_value = authored->specular_val;
    out->legacy_ambient = authored->ambient;
    out->normal_scale = authored->nm_intensity;
    out->legacy_lightness_boost = config->lightness_boost;
    out->legacy_saturation_boost = config->saturation_boost;
    out->legacy_shadeless = authored->shadeless ? 1 : 0;
    return XWA_XWAU_MATERIAL_RENDER_LEGACY;
}

int XwaXwauNormalMap_Load(AeronVfs* vfs,
                          const char* reference,
                          XwaXwauNormalMapImage* out,
                          char* error, size_t error_size) {
    (void)vfs;
    ++g_normal_load_calls;
    snprintf(g_normal_reference, sizeof g_normal_reference, "%s",
             reference ? reference : "");
    if (error && error_size) error[0] = '\0';

    if (g_normal_fail || !out) {
        if (error && error_size)
            snprintf(error, error_size, "synthetic normal-map load error");
        return 0;
    }

    const int slot = (g_normal_load_calls - 1) & 7;
    g_normal_pixels[slot][0] = 11;
    g_normal_pixels[slot][1] = 22;
    g_normal_pixels[slot][2] = 33;
    g_normal_pixels[slot][3] = 44;
    out->rgba = g_normal_pixels[slot];
    out->width = 1;
    out->height = 1;
    return 1;
}

void XwaXwauNormalMap_Free(XwaXwauNormalMapImage* image) {
    ++g_normal_free_calls;
    if (!image) return;
    if (image->rgba) {
        memset(image->rgba, 0xee, 4);
    }
    memset(image, 0, sizeof *image);
}

typedef struct CapturedOverride {
    int has_name;
    char name[96];
    uint32_t flags;
    float legacy_specular_exponent;
    float legacy_specular_intensity;
    float legacy_specular_color_control;
    float legacy_specular_value;
    float legacy_ambient;
    float normal_scale;
    float legacy_lightness_boost;
    float legacy_saturation_boost;
    int legacy_shadeless;
    uint32_t normal_width;
    uint32_t normal_height;
    uint8_t normal_rgba[4];
    int normal_image_valid;
} CapturedOverride;

static int g_build_calls;
static size_t g_captured_material_count;
static CapturedOverride g_captured[8];
static int g_all_images_alive_during_build;
static float g_captured_smooth_angle;
static float g_captured_emissive_strength;
static int g_captured_emissive;
static int g_captured_max_atlas;
static char g_captured_label[512];

bool Aeron_OptModelBuildMemory(
    const void* bytes,
    size_t size,
    const char* label,
    const AeronOptModelBuildOptions* options,
    AeronFlightModel* out,
    AeronOptModelError* error) {

    (void)bytes;
    (void)size;
    ++g_build_calls;
    g_all_images_alive_during_build = 1;
    if (error) memset(error, 0, sizeof *error);
    if (out) memset(out, 0, sizeof *out);

    if (!options || options->material_override_count > 8)
        return false;

    snprintf(g_captured_label, sizeof g_captured_label, "%s",
             label ? label : "");
    g_captured_smooth_angle = options->smooth_angle_degrees;
    g_captured_emissive_strength = options->emissive_strength;
    g_captured_emissive = options->emissive ? 1 : 0;
    g_captured_max_atlas = options->max_atlas_size;
    g_captured_material_count = options->material_override_count;
    memset(g_captured, 0, sizeof g_captured);

    for (size_t i = 0; i < options->material_override_count; ++i) {
        const AeronOptMaterialOverride* source = &options->material_overrides[i];
        CapturedOverride* dest = &g_captured[i];
        dest->has_name = source->texture_name != NULL;
        if (source->texture_name)
            snprintf(dest->name, sizeof dest->name, "%s", source->texture_name);
        dest->flags = source->flags;
        dest->legacy_specular_exponent = source->legacy_specular_exponent;
        dest->legacy_specular_intensity = source->legacy_specular_intensity;
        dest->legacy_specular_color_control = source->legacy_specular_color_control;
        dest->legacy_specular_value = source->legacy_specular_value;
        dest->legacy_ambient = source->legacy_ambient;
        dest->normal_scale = source->normal_scale;
        dest->legacy_lightness_boost = source->legacy_lightness_boost;
        dest->legacy_saturation_boost = source->legacy_saturation_boost;
        dest->legacy_shadeless = source->legacy_shadeless ? 1 : 0;
        dest->normal_width = source->normal_image.width;
        dest->normal_height = source->normal_image.height;

        if (source->flags & AERON_OPT_MATERIAL_OVERRIDE_NORMAL_IMAGE) {
            if (!source->normal_image.rgba8 ||
                source->normal_image.width != 1 ||
                source->normal_image.height != 1 ||
                source->normal_image.rgba8[0] != 11 ||
                source->normal_image.rgba8[1] != 22 ||
                source->normal_image.rgba8[2] != 33 ||
                source->normal_image.rgba8[3] != 44) {
                g_all_images_alive_during_build = 0;
            } else {
                memcpy(dest->normal_rgba, source->normal_image.rgba8, 4);
                dest->normal_image_valid = 1;
            }
        }
    }

    return true;
}

static void reset_fixture(void) {
    g_model_read_calls = 0;
    g_model_read_path[0] = '\0';
    g_material_mode = MATERIAL_MODE_MISSING;
    g_material_load_calls = 0;
    g_material_free_calls = 0;
    g_material_basename[0] = '\0';
    g_material_resolve_calls = 0;
    memset(g_material_resolve_names, 0, sizeof g_material_resolve_names);
    g_render_config_calls = 0;
    g_render_config_error = 0;
    g_render_resolve_calls = 0;
    g_normal_load_calls = 0;
    g_normal_free_calls = 0;
    g_normal_fail = 0;
    g_normal_reference[0] = '\0';
    memset(g_normal_pixels, 0, sizeof g_normal_pixels);
    g_build_calls = 0;
    g_captured_material_count = 0;
    memset(g_captured, 0, sizeof g_captured);
    g_all_images_alive_during_build = 1;
    g_captured_smooth_angle = 0.0f;
    g_captured_emissive_strength = 0.0f;
    g_captured_emissive = 0;
    g_captured_max_atlas = 0;
    g_captured_label[0] = '\0';
}

static int nearly_equal(float a, float b) {
    return fabsf(a - b) <= 0.0001f;
}

static CapturedOverride* find_override(const char* name) {
    for (size_t i = 0; i < g_captured_material_count; ++i) {
        if (!name && !g_captured[i].has_name)
            return &g_captured[i];
        if (name && g_captured[i].has_name &&
            ascii_equal_ci(g_captured[i].name, name))
            return &g_captured[i];
    }
    return NULL;
}

static uint32_t expected_legacy_flags(void) {
    return AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SPECULAR_EXPONENT |
           AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SPECULAR_INTENSITY |
           AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SPECULAR_COLOR_CONTROL |
           AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SPECULAR_VALUE |
           AERON_OPT_MATERIAL_OVERRIDE_LEGACY_AMBIENT |
           AERON_OPT_MATERIAL_OVERRIDE_NORMAL_SCALE |
           AERON_OPT_MATERIAL_OVERRIDE_LEGACY_LIGHTNESS_BOOST |
           AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SATURATION_BOOST |
           AERON_OPT_MATERIAL_OVERRIDE_LEGACY_SHADELESS;
}

static int test_missing_material_preserves_previous_build(void) {
    reset_fixture();
    g_material_mode = MATERIAL_MODE_MISSING;

    AeronVfs vfs = {0};
    AeronFlightModel model;
    char error[256] = {0};

    if (!XwaRemasterOptMesh_Build(
            &vfs, "XWing", 37.0f, 2.0f,
            &model, error, sizeof error)) {
        fprintf(stderr, "FAIL missing .mat changed previous OPT build: %s\n", error);
        return 0;
    }

    if (g_material_load_calls != 1 ||
        strcmp(g_material_basename, "XWing") != 0) {
        fprintf(stderr,
                "FAIL missing .mat path did not call XwaXwauMaterial_LoadAsset exactly once\n");
        return 0;
    }

    if (g_render_config_calls != 0 ||
        g_material_resolve_calls != 0 ||
        g_render_resolve_calls != 0 ||
        g_normal_load_calls != 0) {
        fprintf(stderr,
                "FAIL missing .mat loaded XWAU render/config/normal state\n");
        return 0;
    }

    if (g_build_calls != 1 || g_captured_material_count != 0) {
        fprintf(stderr,
                "FAIL missing .mat did not preserve zero material overrides\n");
        return 0;
    }

    if (!nearly_equal(g_captured_smooth_angle, 37.0f) ||
        !nearly_equal(g_captured_emissive_strength, 2.0f) ||
        !g_captured_emissive ||
        g_captured_max_atlas != 8192 ||
        strcmp(g_captured_label, "FLIGHTMODELS/XWing.OPT") != 0) {
        fprintf(stderr,
                "FAIL missing .mat changed existing Aeron OPT build options\n");
        return 0;
    }

    return 1;
}

static int test_loaded_material_builds_default_named_and_normal_overrides(void) {
    reset_fixture();
    g_material_mode = MATERIAL_MODE_LOADED;

    AeronVfs vfs = {0};
    AeronFlightModel model;
    char error[256] = {0};

    if (!XwaRemasterOptMesh_Build(
            &vfs, "XWing", 25.0f, 1.5f,
            &model, error, sizeof error)) {
        fprintf(stderr, "FAIL loaded .mat OPT build: %s\n", error);
        return 0;
    }

    if (g_material_load_calls != 1 || g_render_config_calls != 1) {
        fprintf(stderr,
                "FAIL loaded .mat did not load material then render config exactly once\n");
        return 0;
    }

    if (g_material_resolve_calls != 2 || g_render_resolve_calls != 3) {
        fprintf(stderr,
                "FAIL named labels were not deduplicated case-insensitively or default was not resolved\n");
        return 0;
    }

    int saw_hull = 0;
    int saw_tex = 0;
    for (int i = 0; i < g_material_resolve_calls && i < 8; ++i) {
        if (ascii_equal_ci(g_material_resolve_names[i], "Hull")) saw_hull = 1;
        if (ascii_equal_ci(g_material_resolve_names[i], "TEX00007")) saw_tex = 1;
    }
    if (!saw_hull || !saw_tex) {
        fprintf(stderr, "FAIL expected unique XWAU material section labels were not resolved\n");
        return 0;
    }

    if (g_build_calls != 1 || g_captured_material_count != 3) {
        fprintf(stderr,
                "FAIL expected default + 2 named Aeron material overrides, got %zu\n",
                g_captured_material_count);
        return 0;
    }

    CapturedOverride* def = find_override(NULL);
    CapturedOverride* hull = find_override("Hull");
    CapturedOverride* tex = find_override("TEX00007");
    if (!def || !hull || !tex) {
        fprintf(stderr, "FAIL default/named override identity is incorrect\n");
        return 0;
    }

    const uint32_t legacy_flags = expected_legacy_flags();
    const uint32_t forbidden_pbr =
        AERON_OPT_MATERIAL_OVERRIDE_METALLIC_FACTOR |
        AERON_OPT_MATERIAL_OVERRIDE_ROUGHNESS_FACTOR;

    for (size_t i = 0; i < g_captured_material_count; ++i) {
        if ((g_captured[i].flags & legacy_flags) != legacy_flags ||
            (g_captured[i].flags & forbidden_pbr) != 0) {
            fprintf(stderr,
                    "FAIL override %zu omitted legacy fields or invented PBR mapping flags=0x%x\n",
                    i, g_captured[i].flags);
            return 0;
        }
    }

    if (!nearly_equal(def->legacy_specular_exponent, 32.0f) ||
        !nearly_equal(def->legacy_specular_intensity, 0.25f) ||
        !nearly_equal(def->legacy_specular_color_control, 1.75f) ||
        !nearly_equal(def->legacy_specular_value, 0.35f) ||
        !nearly_equal(def->legacy_ambient, 1.3f) ||
        !nearly_equal(def->normal_scale, 2.5f) ||
        !nearly_equal(def->legacy_lightness_boost, 8.0f) ||
        !nearly_equal(def->legacy_saturation_boost, 1.0f) ||
        def->legacy_shadeless != 0) {
        fprintf(stderr, "FAIL default legacy translation was not copied exactly\n");
        return 0;
    }

    if (!nearly_equal(hull->legacy_specular_exponent, 12.8f) ||
        !nearly_equal(hull->legacy_specular_intensity, 0.10f) ||
        !nearly_equal(hull->legacy_specular_color_control, 0.80f) ||
        !nearly_equal(hull->legacy_specular_value, 0.60f) ||
        !nearly_equal(hull->legacy_ambient, 0.40f) ||
        !nearly_equal(hull->normal_scale, 1.25f) ||
        hull->legacy_shadeless != 1) {
        fprintf(stderr, "FAIL named legacy translation was not copied exactly\n");
        return 0;
    }

    if (!(hull->flags & AERON_OPT_MATERIAL_OVERRIDE_NORMAL_IMAGE) ||
        hull->normal_width != 1 || hull->normal_height != 1 ||
        !hull->normal_image_valid || !g_all_images_alive_during_build ||
        memcmp(hull->normal_rgba, (uint8_t[]){11, 22, 33, 44}, 4) != 0) {
        fprintf(stderr,
                "FAIL authored normal image was not alive and attached during Aeron build\n");
        return 0;
    }

    if (g_normal_load_calls != 1 || g_normal_free_calls != 1 ||
        strcmp(g_normal_reference, "Effects\\Test-Normal.dat-42-7") != 0) {
        fprintf(stderr,
                "FAIL authored DAT NormalMap ownership/reference handling is incorrect\n");
        return 0;
    }

    if (g_material_free_calls != 1) {
        fprintf(stderr, "FAIL loaded material parse tree was not released exactly once\n");
        return 0;
    }

    return 1;
}

static int test_material_error_blocks_aeron_build(void) {
    reset_fixture();
    g_material_mode = MATERIAL_MODE_ERROR;

    AeronVfs vfs = {0};
    AeronFlightModel model;
    char error[256] = {0};

    if (XwaRemasterOptMesh_Build(
            &vfs, "XWing", 25.0f, 1.5f,
            &model, error, sizeof error)) {
        fprintf(stderr, "FAIL malformed authored .mat was silently ignored\n");
        return 0;
    }

    if (g_material_load_calls != 1 || g_render_config_calls != 0 ||
        g_build_calls != 0 || error[0] == '\0') {
        fprintf(stderr,
                "FAIL material compatibility error did not stop before render/config/Aeron build\n");
        return 0;
    }

    return 1;
}

static int test_config_error_releases_loaded_material(void) {
    reset_fixture();
    g_material_mode = MATERIAL_MODE_LOADED;
    g_render_config_error = 1;

    AeronVfs vfs = {0};
    AeronFlightModel model;
    char error[256] = {0};

    if (XwaRemasterOptMesh_Build(
            &vfs, "XWing", 25.0f, 1.5f,
            &model, error, sizeof error)) {
        fprintf(stderr, "FAIL malformed authored SSAO.cfg was silently ignored\n");
        return 0;
    }

    if (g_render_config_calls != 1 || g_build_calls != 0 ||
        g_normal_load_calls != 0 || g_material_free_calls != 1 ||
        error[0] == '\0') {
        fprintf(stderr,
                "FAIL config compatibility error ownership/order is incorrect\n");
        return 0;
    }

    return 1;
}

static int test_normal_error_releases_material_and_blocks_aeron(void) {
    reset_fixture();
    g_material_mode = MATERIAL_MODE_LOADED;
    g_normal_fail = 1;

    AeronVfs vfs = {0};
    AeronFlightModel model;
    char error[256] = {0};

    if (XwaRemasterOptMesh_Build(
            &vfs, "XWing", 25.0f, 1.5f,
            &model, error, sizeof error)) {
        fprintf(stderr, "FAIL malformed authored NormalMap was silently ignored\n");
        return 0;
    }

    if (g_normal_load_calls != 1 || g_build_calls != 0 ||
        g_material_free_calls != 1 || error[0] == '\0') {
        fprintf(stderr,
                "FAIL normal-map compatibility error ownership/order is incorrect\n");
        return 0;
    }

    return 1;
}

int main(void) {
    AeronVfs vfs = {0};
    char error[256] = {0};

    if (!XwaRemasterOptMesh_Init(&vfs, error, sizeof error)) {
        fprintf(stderr, "FAIL OPT mesh test initialization: %s\n", error);
        return 1;
    }

    if (!test_missing_material_preserves_previous_build()) return 1;
    if (!test_loaded_material_builds_default_named_and_normal_overrides()) return 1;
    if (!test_material_error_blocks_aeron_build()) return 1;
    if (!test_config_error_releases_loaded_material()) return 1;
    if (!test_normal_error_releases_material_and_blocks_aeron()) return 1;

    puts("PASS: XWAU OPT static-material integration contracts");
    return 0;
}
