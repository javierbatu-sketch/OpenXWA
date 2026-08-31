#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aeron/vfs.h"
#include "xwa_remaster/xwau_material.h"
#include "xwa_remaster/xwau_material_asset.h"

struct AeronVfs {
    int unused;
};

static const char* s_present_path = NULL;
static const char* s_present_text = NULL;
static char s_last_exists_path[512];
static char s_last_read_path[512];

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

static int test_loads_present_material(void) {
    static const char text[] =
        "[Default]\n"
        "Glossiness = 2.5\n"
        "SpecularVal = 0.35\n"
        "\n"
        "[TEX00008]\n"
        "NoBloom = 1\n";

    AeronVfs vfs = {0};
    XwaXwauMaterialFile file;
    XwaXwauMaterialResolved resolved;
    char error[256] = {0};

    memset(&file, 0, sizeof file);
    memset(&resolved, 0, sizeof resolved);
    reset_fixture("Materials/AssaultGunboat.mat", text);

    const XwaXwauMaterialAssetResult result =
        XwaXwauMaterial_LoadAsset(&vfs, "AssaultGunboat",
                                  &file, error, sizeof error);

    if (result != XWA_XWAU_MATERIAL_ASSET_LOADED) {
        fprintf(stderr, "FAIL present load result=%d error=%s\n",
                (int)result, error);
        return 0;
    }
    if (strcmp(s_last_exists_path, "Materials/AssaultGunboat.mat") != 0 ||
        strcmp(s_last_read_path, "Materials/AssaultGunboat.mat") != 0) {
        fprintf(stderr, "FAIL lookup path exists='%s' read='%s'\n",
                s_last_exists_path, s_last_read_path);
        XwaXwauMaterial_Free(&file);
        return 0;
    }
    if (!XwaXwauMaterial_Resolve(&file, "TEX00008",
                                 &resolved, error, sizeof error)) {
        fprintf(stderr, "FAIL resolve loaded material: %s\n", error);
        XwaXwauMaterial_Free(&file);
        return 0;
    }
    if (!resolved.has_glossiness || resolved.glossiness != 2.5f ||
        !resolved.has_specular_val || resolved.specular_val != 0.35f ||
        !resolved.has_no_bloom || !resolved.no_bloom) {
        fprintf(stderr, "FAIL loaded material semantics\n");
        XwaXwauMaterial_Free(&file);
        return 0;
    }

    XwaXwauMaterial_Free(&file);
    return 1;
}

static int test_missing_material_is_not_error(void) {
    AeronVfs vfs = {0};
    XwaXwauMaterialFile file;
    char error[256] = {0};

    memset(&file, 0xA5, sizeof file);
    reset_fixture(NULL, NULL);

    const XwaXwauMaterialAssetResult result =
        XwaXwauMaterial_LoadAsset(&vfs, "OriginalOnlyCraft",
                                  &file, error, sizeof error);

    if (result != XWA_XWAU_MATERIAL_ASSET_MISSING) {
        fprintf(stderr, "FAIL missing result=%d error=%s\n",
                (int)result, error);
        return 0;
    }
    if (strcmp(s_last_exists_path, "Materials/OriginalOnlyCraft.mat") != 0) {
        fprintf(stderr, "FAIL missing lookup path='%s'\n", s_last_exists_path);
        return 0;
    }
    if (s_last_read_path[0] != '\0') {
        fprintf(stderr, "FAIL missing material was read anyway\n");
        return 0;
    }
    if (error[0] != '\0') {
        fprintf(stderr, "FAIL missing material produced error='%s'\n", error);
        return 0;
    }
    if (file.sections != NULL || file.section_count != 0 ||
        file.section_capacity != 0) {
        fprintf(stderr, "FAIL missing material did not zero output\n");
        return 0;
    }

    return 1;
}

static int test_present_invalid_material_is_error(void) {
    static const char invalid_text[] =
        "[Default]\n"
        "Glossiness = definitely-not-a-number\n";

    AeronVfs vfs = {0};
    XwaXwauMaterialFile file;
    char error[256] = {0};

    memset(&file, 0, sizeof file);
    reset_fixture("Materials/BrokenCraft.mat", invalid_text);

    const XwaXwauMaterialAssetResult result =
        XwaXwauMaterial_LoadAsset(&vfs, "BrokenCraft",
                                  &file, error, sizeof error);

    if (result != XWA_XWAU_MATERIAL_ASSET_ERROR) {
        fprintf(stderr, "FAIL invalid result=%d error=%s\n",
                (int)result, error);
        return 0;
    }
    if (error[0] == '\0') {
        fprintf(stderr, "FAIL invalid material did not report error\n");
        return 0;
    }

    return 1;
}

int main(void) {
    if (!test_loads_present_material()) {
        return 1;
    }
    if (!test_missing_material_is_not_error()) {
        return 1;
    }
    if (!test_present_invalid_material_is_error()) {
        return 1;
    }

    puts("PASS: XWAU material asset lookup contracts");
    return 0;
}
