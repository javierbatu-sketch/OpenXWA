#include <math.h>
#include <stdio.h>
#include <string.h>

/*
 * XWAU-004 first RED contract.
 *
 * This test intentionally names the parser/resolver API before production
 * code exists. The first workflow run must fail to compile/link until
 * XWAU-004 implements this contract.
 *
 * Syntax below is copied from real XWAU Materials/*.mat conventions.
 */
#include "xwa_remaster/xwau_material.h"

static int nearf(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

int main(void) {
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
        return 1;
    }

    if (!XwaXwauMaterial_Resolve(&parsed, "TEX00008",
                                 &resolved, error, sizeof error)) {
        fprintf(stderr, "FAIL resolve: %s\n", error);
        XwaXwauMaterial_Free(&parsed);
        return 1;
    }

    if (!resolved.has_glossiness || !nearf(resolved.glossiness, 0.10f)) {
        fprintf(stderr, "FAIL inheritance: Glossiness\n");
        return 1;
    }
    if (!resolved.has_intensity || !nearf(resolved.intensity, 0.40f)) {
        fprintf(stderr, "FAIL inheritance: Intensity\n");
        return 1;
    }
    if (!resolved.has_metallic || !nearf(resolved.metallic, 0.50f)) {
        fprintf(stderr, "FAIL inheritance: Metallic\n");
        return 1;
    }
    if (!resolved.has_ambient || !nearf(resolved.ambient, 0.15f)) {
        fprintf(stderr, "FAIL inheritance: Ambient\n");
        return 1;
    }

    if (!resolved.has_nm_intensity || !nearf(resolved.nm_intensity, 0.50f)) {
        fprintf(stderr, "FAIL override: NMIntensity\n");
        return 1;
    }
    if (!resolved.has_normal_map ||
        strcmp(resolved.normal_map, "Effects\\AssaultGunboat.dat-0-7") != 0) {
        fprintf(stderr, "FAIL override: NormalMap\n");
        return 1;
    }
    if (!resolved.has_no_bloom || !resolved.no_bloom) {
        fprintf(stderr, "FAIL override: NoBloom\n");
        return 1;
    }
    if (!resolved.has_shadeless || !resolved.shadeless) {
        fprintf(stderr, "FAIL override: Shadeless\n");
        return 1;
    }

    XwaXwauMaterial_Free(&parsed);
    puts("PASS: XWAU static material Default inheritance + TEX override");
    return 0;
}
