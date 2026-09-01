#include "xwa_remaster/xwau_normal_map.h"

#include "xwa_formats/xwa_2d/xwa_2d.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int xwau_normal_map_fail(
    char* error,
    size_t error_size,
    const char* format,
    ...) {

    if (error && error_size) {
        va_list args;
        va_start(args, format);
        vsnprintf(error, error_size, format, args);
        va_end(args);
    }

    return 0;
}

static int ascii_equal_ci(char a, char b) {
    return tolower((unsigned char)a) == tolower((unsigned char)b);
}

static const char* find_last_dat_suffix(const char* text) {
    const char* last = NULL;

    if (!text) {
        return NULL;
    }

    for (const char* p = text; p[0] && p[1] && p[2] && p[3] && p[4]; ++p) {
        if (p[0] == '.' &&
            ascii_equal_ci(p[1], 'd') &&
            ascii_equal_ci(p[2], 'a') &&
            ascii_equal_ci(p[3], 't') &&
            p[4] == '-') {
            last = p;
        }
    }

    return last;
}

static int parse_u16_decimal(
    const char* begin,
    const char* end,
    uint16_t* out) {

    unsigned value = 0;

    if (!begin || !end || !out || begin >= end) {
        return 0;
    }

    for (const char* p = begin; p < end; ++p) {
        const unsigned char c = (unsigned char)*p;

        if (c < '0' || c > '9') {
            return 0;
        }

        value = value * 10u + (unsigned)(c - '0');
        if (value > 65535u) {
            return 0;
        }
    }

    *out = (uint16_t)value;
    return 1;
}

int XwaXwauNormalMap_ParseReference(
    const char* text,
    XwaXwauNormalMapReference* out,
    char* error,
    size_t error_size) {

    if (error && error_size) {
        error[0] = '\0';
    }

    if (!out) {
        return xwau_normal_map_fail(
            error, error_size, "invalid XWAU normal-map output");
    }

    memset(out, 0, sizeof *out);

    if (!text || !*text) {
        return xwau_normal_map_fail(
            error, error_size, "empty XWAU normal-map reference");
    }

    const char* dat_suffix = find_last_dat_suffix(text);
    if (!dat_suffix) {
        return xwau_normal_map_fail(
            error, error_size,
            "XWAU normal-map reference must end in .dat-G-S");
    }

    const char* group_begin = dat_suffix + 5;
    const char* separator = strchr(group_begin, '-');
    if (!separator || separator == group_begin) {
        return xwau_normal_map_fail(
            error, error_size,
            "invalid XWAU normal-map group/sprite suffix");
    }

    const char* sprite_begin = separator + 1;
    const char* end = text + strlen(text);
    if (sprite_begin >= end || strchr(sprite_begin, '-')) {
        return xwau_normal_map_fail(
            error, error_size,
            "invalid XWAU normal-map group/sprite suffix");
    }

    uint16_t group = 0;
    uint16_t sprite_id = 0;

    if (!parse_u16_decimal(group_begin, separator, &group) ||
        !parse_u16_decimal(sprite_begin, end, &sprite_id)) {
        return xwau_normal_map_fail(
            error, error_size,
            "invalid XWAU normal-map group/sprite number");
    }

    const size_t dat_path_size = (size_t)(dat_suffix - text) + 4u;
    if (dat_path_size == 4u ||
        dat_path_size >= sizeof out->dat_path) {
        return xwau_normal_map_fail(
            error, error_size,
            "XWAU normal-map DAT path is too long");
    }

    memcpy(out->dat_path, text, dat_path_size);
    out->dat_path[dat_path_size] = '\0';
    out->group = group;
    out->sprite_id = sprite_id;

    return 1;
}

void XwaXwauNormalMap_Free(XwaXwauNormalMapImage* image) {
    if (!image) {
        return;
    }

    free(image->rgba);
    memset(image, 0, sizeof *image);
}

int XwaXwauNormalMap_Load(
    AeronVfs* vfs,
    const char* reference,
    XwaXwauNormalMapImage* out,
    char* error,
    size_t error_size) {

    if (error && error_size) {
        error[0] = '\0';
    }

    if (!out) {
        return xwau_normal_map_fail(
            error, error_size, "invalid XWAU normal-map image output");
    }

    memset(out, 0, sizeof *out);

    if (!vfs) {
        return xwau_normal_map_fail(
            error, error_size, "invalid XWAU normal-map VFS");
    }

    XwaXwauNormalMapReference parsed;
    if (!XwaXwauNormalMap_ParseReference(
            reference, &parsed, error, error_size)) {
        return 0;
    }

    if (!AeronVfs_Exists(
            vfs, AERON_VFS_ROOT_ASSET, parsed.dat_path)) {
        return xwau_normal_map_fail(
            error, error_size,
            "authored XWAU normal-map DAT is missing: %s",
            parsed.dat_path);
    }

    uint8_t* dat_bytes = NULL;
    size_t dat_size = 0;

    /*
     * max_size == 0 is Aeron's documented "no caller limit" mode.
     * XWAU Effects DAT files are authored assets and can be large; the
     * existing DAT decoder remains responsible for structural validation.
     */
    if (!AeronVfs_ReadAll(
            vfs,
            AERON_VFS_ROOT_ASSET,
            parsed.dat_path,
            0,
            &dat_bytes,
            &dat_size) ||
        !dat_bytes ||
        dat_size == 0) {
        free(dat_bytes);
        return xwau_normal_map_fail(
            error, error_size,
            "failed to read authored XWAU normal-map DAT: %s",
            parsed.dat_path);
    }

    Xwa2dFrameSet frames = {0};

    if (!Xwa2d_DatAppendGroup(
            dat_bytes,
            dat_size,
            parsed.group,
            &frames,
            error,
            error_size)) {
        free(dat_bytes);
        Xwa2dFrameSet_Free(&frames);

        if (!error || !error_size || error[0] == '\0') {
            return xwau_normal_map_fail(
                error, error_size,
                "failed to decode XWAU normal-map DAT group %u",
                (unsigned)parsed.group);
        }

        return 0;
    }

    free(dat_bytes);

    if (frames.count == 0) {
        Xwa2dFrameSet_Free(&frames);
        return xwau_normal_map_fail(
            error, error_size,
            "authored XWAU normal-map group %u was not found in %s",
            (unsigned)parsed.group,
            parsed.dat_path);
    }

    for (int i = 0; i < frames.count; ++i) {
        Xwa2dFrame* frame = &frames.frames[i];

        if (frame->sprite_id != (int)parsed.sprite_id) {
            continue;
        }

        if (!frame->rgba || frame->width <= 0 || frame->height <= 0) {
            Xwa2dFrameSet_Free(&frames);
            return xwau_normal_map_fail(
                error, error_size,
                "authored XWAU normal-map sprite %u is invalid",
                (unsigned)parsed.sprite_id);
        }

        out->rgba = frame->rgba;
        out->width = frame->width;
        out->height = frame->height;

        /*
         * Transfer ownership of the selected decoded pixels to the result.
         * The remaining frame-set storage is freed normally.
         */
        frame->rgba = NULL;
        Xwa2dFrameSet_Free(&frames);
        return 1;
    }

    Xwa2dFrameSet_Free(&frames);
    return xwau_normal_map_fail(
        error, error_size,
        "authored XWAU normal-map sprite %u was not found in group %u",
        (unsigned)parsed.sprite_id,
        (unsigned)parsed.group);
}
