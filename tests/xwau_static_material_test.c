#include <math.h>
#include <stdio.h>
#include <string.h>

/*
 * XWAU-004 parser contracts.
 *
 * Syntax below follows real XWAU Materials .mat conventions.
 */
#include "xwa_remaster/xwau_material.h"

static int nearf(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

static int test_default_inheritance_and_group_override(void) {
    static const char material_text[] =
        "[Default]\n"
        "Glossiness = 0.10\n"
        "Intensity = 0.40\n"
        "Metallic = 0.50\n"
        "NMIntensity = 0.40\n"
        "Ambient = 0.15\n"
        "\n"
        "frame_once = IEVT_SHIELDS_DOWN, LYR_SCREEN, Effects\\GenericDamage.dat-2, 40, 0, 0.5, 0\n"
        "\n"
        "[TEX00007,TEX00008,TEX00009,TEX00010]\n"
        "NormalMap = Effects\\AssaultGunboat.dat-0-7\n"
        "NMIntensity = 0.50\n"
        "NoBloom = 1\n"
        "Shadeless = 1\n";

    XwaXwauMaterialFile parsed;
    XwaXwauMaterialResolved resolved;
    char error[256] = {0};

    memset(&parsed, 0, sizeof parsed);
    memset(&resolved, 0, sizeof resolved);

    if (!XwaXwauMaterial_ParseText(material_text, strlen(material_text),
                                   &parsed, error, sizeof error)) {
        fprintf(stderr, "FAIL parse: %s\n", error);
        return 0;
    }

    if (!XwaXwauMaterial_Resolve(&parsed, "TEX00008",
                                 &resolved, error, sizeof error)) {
        fprintf(stderr, "FAIL resolve: %s\n", error);
        XwaXwauMaterial_Free(&parsed);
        return 0;
    }

    if (!resolved.has_glossiness || !nearf(resolved.glossiness, 0.10f) ||
        !resolved.has_intensity || !nearf(resolved.intensity, 0.40f) ||
        !resolved.has_metallic || !nearf(resolved.metallic, 0.50f) ||
        !resolved.has_ambient || !nearf(resolved.ambient, 0.15f) ||
        !resolved.has_nm_intensity || !nearf(resolved.nm_intensity, 0.50f) ||
        !resolved.has_normal_map ||
        strcmp(resolved.normal_map, "Effects\\AssaultGunboat.dat-0-7") != 0 ||
        !resolved.has_no_bloom || !resolved.no_bloom ||
        !resolved.has_shadeless || !resolved.shadeless) {
        fprintf(stderr, "FAIL inheritance/group override contract\n");
        XwaXwauMaterial_Free(&parsed);
        return 0;
    }

    XwaXwauMaterial_Free(&parsed);
    return 1;
}

static int test_multiline_group_section(void) {
    static const char material_text[] =
        "[Default]\n"
        "Metallic = 0.15\n"
        "[TEX00045,TEX00046,TEX00047,TEX00048,TEX00049,TEX00050,TEX00051,TEX00052,\n"
        "TEX00053,TEX00054,TEX00055,TEX00056,TEX00057,TEX00058,TEX00059,TEX00060,\n"
        "TEX00061,TEX00062,TEX00063,TEX00064,TEX00065,TEX00066,TEX00067,TEX00068]\n"
        "Intensity = 0.30\n";

    XwaXwauMaterialFile parsed;
    XwaXwauMaterialResolved resolved;
    char error[256] = {0};

    memset(&parsed, 0, sizeof parsed);
    memset(&resolved, 0, sizeof resolved);

    if (!XwaXwauMaterial_ParseText(material_text, strlen(material_text),
                                   &parsed, error, sizeof error)) {
        fprintf(stderr, "FAIL multiline parse: %s\n", error);
        return 0;
    }
    if (!XwaXwauMaterial_Resolve(&parsed, "tex00068", &resolved,
                                 error, sizeof error)) {
        fprintf(stderr, "FAIL multiline resolve: %s\n", error);
        XwaXwauMaterial_Free(&parsed);
        return 0;
    }
    if (!resolved.has_metallic || !nearf(resolved.metallic, 0.15f) ||
        !resolved.has_intensity || !nearf(resolved.intensity, 0.30f)) {
        fprintf(stderr, "FAIL multiline group section values\n");
        XwaXwauMaterial_Free(&parsed);
        return 0;
    }

    XwaXwauMaterial_Free(&parsed);
    return 1;
}

int main(void) {
    if (!test_default_inheritance_and_group_override()) {
        return 1;
    }
    if (!test_multiline_group_section()) {
        return 1;
    }

    puts("PASS: XWAU static material parser contracts");
    return 0;
}
