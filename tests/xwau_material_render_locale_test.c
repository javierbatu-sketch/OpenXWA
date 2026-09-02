#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aeron/vfs.h"
#include "xwa_remaster/xwau_material_render.h"

int AeronVfs_Exists(AeronVfs* vfs, AeronVfsRoot root, const char* path) {
    (void)vfs;
    (void)root;
    (void)path;
    return 0;
}

int AeronVfs_ReadAll(AeronVfs* vfs, AeronVfsRoot root, const char* path,
                     size_t max_size, uint8_t** out_data, size_t* out_size) {
    (void)vfs;
    (void)root;
    (void)path;
    (void)max_size;
    (void)out_data;
    (void)out_size;
    return 0;
}

float __real_strtof(const char* value, char** end);

/* Reproduce the observed Spanish_Spain.1252 MSVCRT decimal-comma behavior
 * without depending on locale packages installed on the CI host. */
float __wrap_strtof(const char* value, char** end) {
    if (value && strcmp(value, "0.25") == 0) {
        if (end) {
            *end = (char*)value + 1;
        }
        return 0.0f;
    }
    return __real_strtof(value, end);
}

int main(void) {
    static const char config_text[] =
        "specular_intensity = 0.25\n";

    XwaXwauMaterialRenderConfig config;
    char error[256] = {0};

    XwaXwauMaterialRenderConfig_InitDefaults(&config);
    if (!XwaXwauMaterialRenderConfig_ParseText(
            config_text, sizeof config_text - 1u,
            &config, error, sizeof error)) {
        fprintf(stderr, "FAIL locale-independent SSAO.cfg parse: %s\n", error);
        return 1;
    }

    if (fabsf(config.specular_intensity - 0.25f) > 0.0001f) {
        fprintf(stderr, "FAIL specular_intensity=%g expected 0.25\n",
                (double)config.specular_intensity);
        return 1;
    }

    puts("PASS: XWAU SSAO.cfg decimals ignore process locale");
    return 0;
}
