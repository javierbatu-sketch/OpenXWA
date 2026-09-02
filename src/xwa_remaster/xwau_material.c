#include "xwa_remaster/xwau_material.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void xwau_material_error(char* error, size_t error_size, const char* message) {
    if (error && error_size) {
        snprintf(error, error_size, "%s", message ? message : "XWAU material error");
    }
}

static char* xwau_trim(char* text) {
    while (*text && isspace((unsigned char)*text)) {
        ++text;
    }

    char* end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }

    return text;
}

static int xwau_ascii_equal(const char* left, const char* right) {
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

static int xwau_parse_float(const char* value, float* out) {
    if (!value || !out) {
        return 0;
    }

    const unsigned char* cursor = (const unsigned char*)value;
    int negative = 0;
    if (*cursor == '+' || *cursor == '-') {
        negative = *cursor == '-';
        ++cursor;
    }

    long double parsed = 0.0L;
    int digit_count = 0;

    while (isdigit(*cursor)) {
        parsed = parsed * 10.0L + (long double)(*cursor - '0');
        if (!isfinite(parsed)) {
            return 0;
        }
        ++digit_count;
        ++cursor;
    }

    if (*cursor == '.') {
        ++cursor;
        long double place = 0.1L;
        while (isdigit(*cursor)) {
            parsed += (long double)(*cursor - '0') * place;
            place *= 0.1L;
            ++digit_count;
            ++cursor;
        }
    }

    if (digit_count == 0) {
        return 0;
    }

    int exponent = 0;
    int exponent_negative = 0;
    if (*cursor == 'e' || *cursor == 'E') {
        ++cursor;
        if (*cursor == '+' || *cursor == '-') {
            exponent_negative = *cursor == '-';
            ++cursor;
        }

        int exponent_digits = 0;
        while (isdigit(*cursor)) {
            if (exponent < 100000) {
                exponent = exponent * 10 + (*cursor - '0');
            }
            ++exponent_digits;
            ++cursor;
        }
        if (exponent_digits == 0) {
            return 0;
        }
    }

    if (*cursor != '\0') {
        return 0;
    }

    if (exponent != 0 && parsed != 0.0L) {
        const int signed_exponent = exponent_negative ? -exponent : exponent;
        parsed *= powl(10.0L, (long double)signed_exponent);
    }
    if (negative) {
        parsed = -parsed;
    }

    const float narrowed = (float)parsed;
    if (!isfinite(narrowed)) {
        return 0;
    }

    *out = narrowed;
    return 1;
}

static int xwau_parse_bool(const char* value, int* out) {
    float parsed = 0.0f;
    if (!xwau_parse_float(value, &parsed)) {
        return 0;
    }
    if (parsed == 0.0f) {
        *out = 0;
        return 1;
    }
    if (parsed == 1.0f) {
        *out = 1;
        return 1;
    }
    return 0;
}

static int xwau_set_float(const char* key, const char* value,
                          XwaXwauMaterialResolved* values,
                          char* error, size_t error_size) {
    float parsed = 0.0f;

#define XWAU_FLOAT_PROPERTY(KEY, HAS_FIELD, VALUE_FIELD) \
    if (xwau_ascii_equal(key, KEY)) { \
        if (!xwau_parse_float(value, &parsed)) { \
            xwau_material_error(error, error_size, "invalid XWAU material numeric value"); \
            return -1; \
        } \
        values->HAS_FIELD = 1; \
        values->VALUE_FIELD = parsed; \
        return 1; \
    }

    XWAU_FLOAT_PROPERTY("Glossiness", has_glossiness, glossiness)
    XWAU_FLOAT_PROPERTY("Intensity", has_intensity, intensity)
    XWAU_FLOAT_PROPERTY("Metallic", has_metallic, metallic)
    XWAU_FLOAT_PROPERTY("NMIntensity", has_nm_intensity, nm_intensity)
    XWAU_FLOAT_PROPERTY("Ambient", has_ambient, ambient)
    XWAU_FLOAT_PROPERTY("SpecularVal", has_specular_val, specular_val)

#undef XWAU_FLOAT_PROPERTY

    return 0;
}

static int xwau_set_property(const char* key, const char* value,
                             XwaXwauMaterialResolved* values,
                             char* error, size_t error_size) {
    const int numeric = xwau_set_float(key, value, values, error, error_size);
    if (numeric != 0) {
        return numeric;
    }

    if (xwau_ascii_equal(key, "NormalMap")) {
        if (!value || !*value || strlen(value) >= sizeof values->normal_map) {
            xwau_material_error(error, error_size, "invalid XWAU material NormalMap");
            return -1;
        }
        values->has_normal_map = 1;
        snprintf(values->normal_map, sizeof values->normal_map, "%s", value);
        return 1;
    }

    if (xwau_ascii_equal(key, "NoBloom")) {
        if (!xwau_parse_bool(value, &values->no_bloom)) {
            xwau_material_error(error, error_size, "invalid XWAU material NoBloom");
            return -1;
        }
        values->has_no_bloom = 1;
        return 1;
    }

    if (xwau_ascii_equal(key, "Shadeless")) {
        if (!xwau_parse_bool(value, &values->shadeless)) {
            xwau_material_error(error, error_size, "invalid XWAU material Shadeless");
            return -1;
        }
        values->has_shadeless = 1;
        return 1;
    }

    if (xwau_ascii_equal(key, "AlphaIsntGlass")) {
        if (!xwau_parse_bool(value, &values->alpha_isnt_glass)) {
            xwau_material_error(error, error_size, "invalid XWAU material AlphaIsntGlass");
            return -1;
        }
        values->has_alpha_isnt_glass = 1;
        return 1;
    }

    return 0;
}

static int xwau_section_add_name(XwaXwauMaterialSection* section, const char* name) {
    if (!section || !name || !*name || strlen(name) >= XWA_XWAU_MATERIAL_NAME_MAX) {
        return 0;
    }

    char** grown = (char**)realloc(section->names,
                                   (section->name_count + 1u) * sizeof *section->names);
    if (!grown) {
        return 0;
    }
    section->names = grown;

    const size_t length = strlen(name);
    char* copy = (char*)malloc(length + 1u);
    if (!copy) {
        return 0;
    }
    memcpy(copy, name, length + 1u);
    section->names[section->name_count++] = copy;
    return 1;
}

static XwaXwauMaterialSection* xwau_file_add_section(XwaXwauMaterialFile* file) {
    if (file->section_count == file->section_capacity) {
        size_t capacity = file->section_capacity ? file->section_capacity * 2u : 8u;
        XwaXwauMaterialSection* grown = (XwaXwauMaterialSection*)realloc(
            file->sections, capacity * sizeof *file->sections);
        if (!grown) {
            return NULL;
        }
        memset(grown + file->section_capacity, 0,
               (capacity - file->section_capacity) * sizeof *grown);
        file->sections = grown;
        file->section_capacity = capacity;
    }

    XwaXwauMaterialSection* section = &file->sections[file->section_count++];
    memset(section, 0, sizeof *section);
    return section;
}

static int xwau_parse_section_names(char* section_text,
                                    XwaXwauMaterialSection* section) {
    char* cursor = section_text;
    while (cursor && *cursor) {
        char* comma = strchr(cursor, ',');
        if (comma) {
            *comma = '\0';
        }

        char* name = xwau_trim(cursor);
        if (!xwau_section_add_name(section, name)) {
            return 0;
        }

        cursor = comma ? comma + 1 : NULL;
    }

    return section->name_count != 0;
}

static int xwau_select_section(char* section_text,
                               XwaXwauMaterialFile* file,
                               XwaXwauMaterialResolved** current,
                               char* error, size_t error_size) {
    section_text = xwau_trim(section_text);
    if (!*section_text) {
        xwau_material_error(error, error_size, "invalid empty XWAU material section");
        return 0;
    }

    if (xwau_ascii_equal(section_text, "Default")) {
        *current = &file->defaults;
        return 1;
    }

    XwaXwauMaterialSection* section = xwau_file_add_section(file);
    if (!section || !xwau_parse_section_names(section_text, section)) {
        xwau_material_error(error, error_size, "could not allocate XWAU material section");
        return 0;
    }

    *current = &section->values;
    return 1;
}

static void xwau_overlay(XwaXwauMaterialResolved* destination,
                         const XwaXwauMaterialResolved* source) {
#define XWAU_OVERLAY(HAS_FIELD, VALUE_FIELD) \
    if (source->HAS_FIELD) { \
        destination->HAS_FIELD = 1; \
        destination->VALUE_FIELD = source->VALUE_FIELD; \
    }

    XWAU_OVERLAY(has_glossiness, glossiness)
    XWAU_OVERLAY(has_intensity, intensity)
    XWAU_OVERLAY(has_metallic, metallic)
    XWAU_OVERLAY(has_nm_intensity, nm_intensity)
    XWAU_OVERLAY(has_ambient, ambient)
    XWAU_OVERLAY(has_specular_val, specular_val)
    XWAU_OVERLAY(has_no_bloom, no_bloom)
    XWAU_OVERLAY(has_shadeless, shadeless)
    XWAU_OVERLAY(has_alpha_isnt_glass, alpha_isnt_glass)

#undef XWAU_OVERLAY

    if (source->has_normal_map) {
        destination->has_normal_map = 1;
        snprintf(destination->normal_map, sizeof destination->normal_map, "%s",
                 source->normal_map);
    }
}

int XwaXwauMaterial_ParseText(const char* text, size_t size,
                              XwaXwauMaterialFile* out,
                              char* error, size_t error_size) {
    if (error && error_size) {
        error[0] = '\0';
    }
    if (!text || !out) {
        xwau_material_error(error, error_size, "invalid XWAU material parse arguments");
        return 0;
    }

    memset(out, 0, sizeof *out);

    char* copy = (char*)malloc(size + 1u);
    if (!copy) {
        xwau_material_error(error, error_size, "out of memory parsing XWAU material");
        return 0;
    }
    memcpy(copy, text, size);
    copy[size] = '\0';

    XwaXwauMaterialResolved* current = NULL;
    char* cursor = copy;
    char section_buffer[4096];
    size_t section_length = 0;
    int collecting_section = 0;

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

        line = xwau_trim(line);
        if (!*line || *line == ';' || *line == '#') {
            continue;
        }

        if (collecting_section) {
            const size_t line_length = strlen(line);
            if (section_length + line_length + 1u > sizeof section_buffer) {
                free(copy);
                XwaXwauMaterial_Free(out);
                xwau_material_error(error, error_size, "XWAU material section is too long");
                return 0;
            }
            memcpy(section_buffer + section_length, line, line_length + 1u);
            section_length += line_length;

            char* close = strchr(section_buffer, ']');
            if (!close) {
                continue;
            }
            *close = '\0';
            if (!xwau_select_section(section_buffer + 1, out, &current,
                                     error, error_size)) {
                free(copy);
                XwaXwauMaterial_Free(out);
                return 0;
            }
            collecting_section = 0;
            section_length = 0;
            continue;
        }

        if (*line == '[') {
            char* close = strchr(line + 1, ']');
            if (close) {
                *close = '\0';
                if (!xwau_select_section(line + 1, out, &current,
                                         error, error_size)) {
                    free(copy);
                    XwaXwauMaterial_Free(out);
                    return 0;
                }
            } else {
                const size_t line_length = strlen(line);
                if (line_length + 1u > sizeof section_buffer) {
                    free(copy);
                    XwaXwauMaterial_Free(out);
                    xwau_material_error(error, error_size, "XWAU material section is too long");
                    return 0;
                }
                memcpy(section_buffer, line, line_length + 1u);
                section_length = line_length;
                collecting_section = 1;
            }
            continue;
        }

        char* equals = strchr(line, '=');
        if (!equals || !current) {
            continue;
        }

        *equals = '\0';
        char* key = xwau_trim(line);
        char* value = xwau_trim(equals + 1);

        char* comment = strchr(value, ';');
        if (comment) {
            *comment = '\0';
            value = xwau_trim(value);
        }

        if (xwau_set_property(key, value, current, error, error_size) < 0) {
            free(copy);
            XwaXwauMaterial_Free(out);
            return 0;
        }
    }

    if (collecting_section) {
        free(copy);
        XwaXwauMaterial_Free(out);
        xwau_material_error(error, error_size, "unterminated XWAU material section");
        return 0;
    }

    free(copy);
    return 1;
}

int XwaXwauMaterial_Resolve(const XwaXwauMaterialFile* file,
                            const char* material_name,
                            XwaXwauMaterialResolved* out,
                            char* error, size_t error_size) {
    if (error && error_size) {
        error[0] = '\0';
    }
    if (!file || !material_name || !*material_name || !out) {
        xwau_material_error(error, error_size, "invalid XWAU material resolve arguments");
        return 0;
    }

    memset(out, 0, sizeof *out);
    xwau_overlay(out, &file->defaults);

    for (size_t section_index = 0; section_index < file->section_count; ++section_index) {
        const XwaXwauMaterialSection* section = &file->sections[section_index];
        int matches = 0;
        for (size_t name_index = 0; name_index < section->name_count; ++name_index) {
            if (xwau_ascii_equal(section->names[name_index], material_name)) {
                matches = 1;
                break;
            }
        }
        if (matches) {
            xwau_overlay(out, &section->values);
        }
    }

    return 1;
}

void XwaXwauMaterial_Free(XwaXwauMaterialFile* file) {
    if (!file) {
        return;
    }

    for (size_t section_index = 0; section_index < file->section_count; ++section_index) {
        XwaXwauMaterialSection* section = &file->sections[section_index];
        for (size_t name_index = 0; name_index < section->name_count; ++name_index) {
            free(section->names[name_index]);
        }
        free(section->names);
    }
    free(file->sections);
    memset(file, 0, sizeof *file);
}
