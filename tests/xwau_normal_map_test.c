#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aeron/vfs.h"
#include "xwa_remaster/xwau_normal_map.h"

#define TEST_GROUP_ID 42u
#define TEST_SPRITE_FIRST 3u
#define TEST_SPRITE_WANTED 7u

struct AeronVfs {
    int unused;
};

static const char* s_present_path = NULL;
static const uint8_t* s_present_bytes = NULL;
static size_t s_present_size = 0;
static char s_last_exists_path[512];
static char s_last_read_path[512];
static AeronVfsRoot s_last_root = AERON_VFS_ROOT_COUNT;

static int path_char_equal(char a, char b) {
    if ((a == '\\' || a == '/') && (b == '\\' || b == '/')) {
        return 1;
    }
    return tolower((unsigned char)a) == tolower((unsigned char)b);
}

static int path_equal(const char* a, const char* b) {
    if (!a || !b) {
        return 0;
    }
    while (*a && *b) {
        if (!path_char_equal(*a, *b)) {
            return 0;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

int AeronVfs_Exists(AeronVfs* vfs, AeronVfsRoot root, const char* path) {
    (void)vfs;
    s_last_root = root;
    snprintf(s_last_exists_path, sizeof s_last_exists_path, "%s",
             path ? path : "");
    return root == AERON_VFS_ROOT_ASSET &&
           s_present_path &&
           path_equal(path, s_present_path);
}

int AeronVfs_ReadAll(AeronVfs* vfs, AeronVfsRoot root, const char* path,
                     size_t max_size, uint8_t** out_data, size_t* out_size) {
    (void)vfs;

    if (!out_data || !out_size || !path) {
        return 0;
    }

    s_last_root = root;
    snprintf(s_last_read_path, sizeof s_last_read_path, "%s", path);
    *out_data = NULL;
    *out_size = 0;

    if (root != AERON_VFS_ROOT_ASSET ||
        !s_present_path ||
        !s_present_bytes ||
        !path_equal(path, s_present_path) ||
        (max_size != 0 && s_present_size > max_size)) {
        return 0;
    }

    uint8_t* copy = (uint8_t*)malloc(s_present_size);
    if (!copy) {
        return 0;
    }

    memcpy(copy, s_present_bytes, s_present_size);
    *out_data = copy;
    *out_size = s_present_size;
    return 1;
}

static void reset_vfs_fixture(const char* path,
                              const uint8_t* bytes,
                              size_t size) {
    s_present_path = path;
    s_present_bytes = bytes;
    s_present_size = size;
    s_last_exists_path[0] = '\0';
    s_last_read_path[0] = '\0';
    s_last_root = AERON_VFS_ROOT_COUNT;
}

static void put_u16(uint8_t* p, uint16_t value) {
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t* p, uint32_t value) {
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)((value >> 8) & 0xffu);
    p[2] = (uint8_t)((value >> 16) & 0xffu);
    p[3] = (uint8_t)((value >> 24) & 0xffu);
}

static void build_sprite(uint8_t* sprite,
                         uint16_t sprite_id,
                         const uint8_t bgra[4]) {
    enum {
        sprite_header_size = 18,
        payload_header_size = 44,
        payload_size = payload_header_size + 4
    };

    memset(sprite, 0, sprite_header_size + payload_size);

    put_u16(sprite + 0, 25);
    put_u16(sprite + 2, 1);
    put_u16(sprite + 4, 1);
    put_u16(sprite + 10, TEST_GROUP_ID);
    put_u16(sprite + 12, sprite_id);
    put_u32(sprite + 14, payload_size);

    uint8_t* payload = sprite + sprite_header_size;
    put_u32(payload + 8, payload_header_size);
    put_u32(payload + 40, 0);
    memcpy(payload + payload_header_size, bgra, 4);
}

static size_t build_two_sprite_dat(uint8_t out[190]) {
    enum {
        dat_header_size = 10,
        directory_record_size = 24,
        directory_record_count = 2,
        sprite_header_size = 18,
        payload_size = 48,
        sprite_size = sprite_header_size + payload_size,
        sprite_offset =
            dat_header_size +
            directory_record_size * directory_record_count
    };

    static const uint8_t first_bgra[4] = {30, 20, 10, 255};
    static const uint8_t wanted_bgra[4] = {60, 50, 40, 128};

    memset(out, 0, 190);

    /* Directory format 1 => 24-byte records. */
    put_u16(out + 8, 1);

    uint8_t* directory_header = out + 10;
    put_u16(directory_header, 1);

    uint8_t* entry = directory_header + directory_record_size;
    put_u16(entry + 0, TEST_GROUP_ID);
    put_u16(entry + 2, 2);
    put_u32(entry + 20, 0);

    build_sprite(out + sprite_offset, TEST_SPRITE_FIRST, first_bgra);
    build_sprite(out + sprite_offset + sprite_size,
                 TEST_SPRITE_WANTED, wanted_bgra);

    return 190;
}

static int test_reference_parser_uses_suffix_after_dat(void) {
    static const char reference[] =
        "Effects\\Assault-Gunboat.DAT-42-7";

    XwaXwauNormalMapReference parsed;
    char error[256] = {0};

    memset(&parsed, 0, sizeof parsed);

    if (!XwaXwauNormalMap_ParseReference(
            reference, &parsed, error, sizeof error)) {
        fprintf(stderr, "FAIL valid normal-map reference: %s\n", error);
        return 0;
    }

    if (strcmp(parsed.dat_path, "Effects\\Assault-Gunboat.DAT") != 0 ||
        parsed.group != TEST_GROUP_ID ||
        parsed.sprite_id != TEST_SPRITE_WANTED) {
        fprintf(stderr,
                "FAIL parsed path='%s' group=%u sprite=%u\n",
                parsed.dat_path,
                (unsigned)parsed.group,
                (unsigned)parsed.sprite_id);
        return 0;
    }

    return 1;
}

static int require_bad_reference(const char* reference) {
    XwaXwauNormalMapReference parsed;
    char error[256] = {0};

    memset(&parsed, 0, sizeof parsed);

    if (XwaXwauNormalMap_ParseReference(
            reference, &parsed, error, sizeof error)) {
        fprintf(stderr, "FAIL malformed reference accepted: %s\n", reference);
        return 0;
    }

    if (error[0] == '\0') {
        fprintf(stderr, "FAIL malformed reference had no error: %s\n",
                reference);
        return 0;
    }

    return 1;
}

static int test_malformed_references_are_errors(void) {
    return require_bad_reference("Effects\\AssaultGunboat.dat-0") &&
           require_bad_reference("Effects\\AssaultGunboat.dat-x-7") &&
           require_bad_reference("Effects\\AssaultGunboat.dat--7") &&
           require_bad_reference("Effects\\AssaultGunboat.dat-70000-7") &&
           require_bad_reference("Effects\\AssaultGunboat.dat-0-70000") &&
           require_bad_reference("Effects\\AssaultGunboat.png-0-7");
}

static int test_load_selects_sprite_id_not_dense_index(void) {
    static const char dat_path[] =
        "Effects\\Assault-Gunboat.dat";
    static const char reference[] =
        "Effects\\Assault-Gunboat.dat-42-7";
    static const uint8_t expected_rgba[4] = {40, 50, 60, 128};

    uint8_t dat[190];
    const size_t dat_size = build_two_sprite_dat(dat);

    AeronVfs vfs = {0};
    XwaXwauNormalMapImage image;
    char error[256] = {0};

    memset(&image, 0, sizeof image);
    reset_vfs_fixture(dat_path, dat, dat_size);

    if (!XwaXwauNormalMap_Load(
            &vfs, reference, &image, error, sizeof error)) {
        fprintf(stderr, "FAIL load authored normal map: %s\n", error);
        return 0;
    }

    if (s_last_root != AERON_VFS_ROOT_ASSET ||
        !path_equal(s_last_read_path, dat_path)) {
        fprintf(stderr,
                "FAIL DAT not read from asset root path='%s' root=%d\n",
                s_last_read_path, (int)s_last_root);
        XwaXwauNormalMap_Free(&image);
        return 0;
    }

    if (!image.rgba ||
        image.width != 1 ||
        image.height != 1 ||
        memcmp(image.rgba, expected_rgba, sizeof expected_rgba) != 0) {
        fprintf(stderr,
                "FAIL selected image width=%u height=%u rgba=%p\n",
                (unsigned)image.width,
                (unsigned)image.height,
                (void*)image.rgba);
        XwaXwauNormalMap_Free(&image);
        return 0;
    }

    /*
     * The DAT contains only two dense frames (IDs 3 and 7). Requesting ID 7
     * therefore proves the resolver used frame.sprite_id rather than frames[7].
     */
    XwaXwauNormalMap_Free(&image);

    if (image.rgba != NULL || image.width != 0 || image.height != 0) {
        fprintf(stderr, "FAIL image free did not clear ownership\n");
        return 0;
    }

    return 1;
}

static int require_load_error(const char* label,
                              const char* present_path,
                              const uint8_t* bytes,
                              size_t size,
                              const char* reference) {
    AeronVfs vfs = {0};
    XwaXwauNormalMapImage image;
    char error[256] = {0};

    memset(&image, 0xA5, sizeof image);
    reset_vfs_fixture(present_path, bytes, size);

    if (XwaXwauNormalMap_Load(
            &vfs, reference, &image, error, sizeof error)) {
        fprintf(stderr, "FAIL %s unexpectedly loaded\n", label);
        XwaXwauNormalMap_Free(&image);
        return 0;
    }

    if (error[0] == '\0') {
        fprintf(stderr, "FAIL %s produced no compatibility error\n", label);
        return 0;
    }

    return 1;
}

static int test_authored_missing_resources_are_errors(void) {
    static const char dat_path[] = "Effects\\AssaultGunboat.dat";
    uint8_t dat[190];
    const size_t dat_size = build_two_sprite_dat(dat);

    if (!require_load_error(
            "missing DAT",
            NULL, NULL, 0,
            "Effects\\Missing.dat-42-7")) {
        return 0;
    }

    if (!require_load_error(
            "missing group",
            dat_path, dat, dat_size,
            "Effects\\AssaultGunboat.dat-43-7")) {
        return 0;
    }

    if (!require_load_error(
            "missing sprite",
            dat_path, dat, dat_size,
            "Effects\\AssaultGunboat.dat-42-8")) {
        return 0;
    }

    if (!require_load_error(
            "malformed authored reference",
            dat_path, dat, dat_size,
            "Effects\\AssaultGunboat.dat-not-a-group-7")) {
        return 0;
    }

    return 1;
}

int main(void) {
    if (!test_reference_parser_uses_suffix_after_dat()) {
        return 1;
    }
    if (!test_malformed_references_are_errors()) {
        return 1;
    }
    if (!test_load_selects_sprite_id_not_dense_index()) {
        return 1;
    }
    if (!test_authored_missing_resources_are_errors()) {
        return 1;
    }

    puts("PASS: XWAU DAT-backed normal-map contracts");
    return 0;
}
