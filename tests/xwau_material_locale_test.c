#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xwa_remaster/xwau_material.h"

/* GNU ld --wrap=strtof sends production strtof calls here.  Reproduce the
 * observed Spanish_Spain.1252 MSVCRT behavior without depending on host
 * locale availability: "0.25" parses as 0 and leaves ".25". */
float __wrap_strtof(const char* value, char** end) {
    if (value && strcmp(value, "0.25") == 0) {
        if (end) {
            *end = (char*)value + 1;
        }
        return 0.0f;
    }

    if (value && strcmp(value, "1") == 0) {
        if (end) {
            *end = (char*)value + 1;
        }
        return 1.0f;
    }

    if (end) {
        *end = (char*)value;
    }
    return 0.0f;
}

int main(void) {
    static const char material_text[] =
        "[Default]\n"
        "Metallic = 0.25\n";

    XwaXwauMaterialFile parsed;
    char error[256] = {0};
    memset(&parsed, 0, sizeof parsed);

    if (!XwaXwauMaterial_ParseText(material_text, strlen(material_text),
                                   &parsed, error, sizeof error)) {
        fprintf(stderr, "FAIL locale-independent parse: %s\n", error);
        return 1;
    }

    if (!parsed.defaults.has_metallic ||
        fabsf(parsed.defaults.metallic - 0.25f) > 0.0001f) {
        fprintf(stderr, "FAIL Metallic != 0.25\n");
        XwaXwauMaterial_Free(&parsed);
        return 1;
    }

    XwaXwauMaterial_Free(&parsed);
    puts("PASS: XWAU material decimals ignore process locale");
    return 0;
}
