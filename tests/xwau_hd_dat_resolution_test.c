#include "xwa_remaster/original_2d.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_GROUP_ID 42
#define TEST_SPRITE_ID 7
#define TEST_ROOT "xwau_hd_dat_test_assets"

struct AeronVfs {
    const char* asset_root;
    const char* user_root;
};

static const char* fake_root(AeronVfs* vfs, AeronVfsRoot root) {
    if (root == AERON_VFS_ROOT_USER)
        return vfs->user_root;
    return vfs->asset_root;
}

static int fake_path(
    AeronVfs* vfs,
    AeronVfsRoot root,
    const char* relative,
    char* out,
    size_t out_size) {
    const char* base = fake_root(vfs, root);
    int written;

    if (!base || !relative || !out || out_size == 0)
        return 0;

    written = snprintf(out, out_size, "%s/%s", base, relative);
    if (written < 0 || (size_t)written >= out_size)
        return 0;

    for (char* p = out; *p; ++p) {
        if (*p == '\\')
            *p = '/';
    }

    return 1;
}

int AeronVfs_Exists(AeronVfs* vfs, AeronVfsRoot root, const char* path) {
    char full[1024];
    FILE* stream;

    if (!fake_path(vfs, root, path, full, sizeof full))
        return 0;

    stream = fopen(full, "rb");
    if (!stream)
        return 0;

    fclose(stream);
    return 1;
}

int AeronVfs_ReadAll(
    AeronVfs* vfs,
    AeronVfsRoot root,
    const char* path,
    size_t max_size,
    uint8_t** out_data,
    size_t* out_size) {
    char full[1024];
    FILE* stream;
    long length;
    uint8_t* data;

    if (!out_data || !out_size ||
        !fake_path(vfs, root, path, full, sizeof full))
        return 0;

    *out_data = NULL;
    *out_size = 0;

    stream = fopen(full, "rb");
    if (!stream)
        return 0;

    if (fseek(stream, 0, SEEK_END) != 0) {
        fclose(stream);
        return 0;
    }

    length = ftell(stream);
    if (length <= 0 ||
        (max_size != 0 && (size_t)length > max_size) ||
        fseek(stream, 0, SEEK_SET) != 0) {
        fclose(stream);
        return 0;
    }

    data = (uint8_t*)malloc((size_t)length);
    if (!data) {
        fclose(stream);
        return 0;
    }

    if (fread(data, 1, (size_t)length, stream) != (size_t)length) {
        free(data);
        fclose(stream);
        return 0;
    }

    fclose(stream);
    *out_data = data;
    *out_size = (size_t)length;
    return 1;
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

static size_t build_dat(uint8_t out[124], const uint8_t bgra[4]) {
    enum {
        dat_header_size = 10,
        directory_record_size = 24,
        directory_record_count = 2,
        sprite_header_size = 18,
        payload_header_size = 44,
        payload_size = payload_header_size + 4,
        sprite_offset =
            dat_header_size +
            directory_record_size * directory_record_count
    };

    uint8_t* directory_header;
    uint8_t* entry;
    uint8_t* sprite;
    uint8_t* payload;

    memset(out, 0, 124);

    /* DAT directory format 1 -> 24-byte directory records. */
    put_u16(out + 8, 1);

    directory_header = out + 10;
    put_u16(directory_header, 1);

    entry = directory_header + directory_record_size;
    put_u16(entry + 0, TEST_GROUP_ID);
    put_u16(entry + 2, 1);
    put_u32(entry + 20, 0);

    sprite = out + sprite_offset;
    put_u16(sprite + 0, 25);
    put_u16(sprite + 2, 1);
    put_u16(sprite + 4, 1);
    put_u16(sprite + 10, TEST_GROUP_ID);
    put_u16(sprite + 12, TEST_SPRITE_ID);
    put_u32(sprite + 14, payload_size);

    payload = sprite + sprite_header_size;
    put_u32(payload + 8, payload_header_size);
    put_u32(payload + 40, 0);
    memcpy(payload + payload_header_size, bgra, 4);

    return 124;
}

static int write_bytes(
    const char* root,
    const char* name,
    const void* bytes,
    size_t size) {
    char path[1024];
    int written = snprintf(path, sizeof path, "%s/%s", root, name);
    FILE* stream;

    if (written < 0 || (size_t)written >= sizeof path)
        return 0;

    stream = fopen(path, "wb");
    if (!stream)
        return 0;

    if (fwrite(bytes, 1, size, stream) != size) {
        fclose(stream);
        return 0;
    }

    return fclose(stream) == 0;
}

static void clean_test_root(void) {
    remove(TEST_ROOT "/RESDATA.TXT");
    remove(TEST_ROOT "/TEST.DAT");
    remove(TEST_ROOT "/TEST_HD.DAT");
    rmdir(TEST_ROOT);
}

static int prepare_root(void) {
    clean_test_root();
    if (mkdir(TEST_ROOT, 0700) != 0 && errno != EEXIST)
        return 0;

    return write_bytes(
        TEST_ROOT,
        "RESDATA.TXT",
        "TEST.DAT\n",
        sizeof("TEST.DAT\n") - 1);
}

static XwaRemasterOriginal2dLoadStatus load_group(
    Xwa2dFrameSet* frames,
    char* error,
    size_t error_size) {
    struct AeronVfs vfs = {
        .asset_root = TEST_ROOT,
        .user_root = TEST_ROOT
    };
    XwaRemasterOriginal2d* reader =
        XwaRemasterOriginal2d_Create((AeronVfs*)&vfs);
    XwaRemasterOriginal2dLoadStatus status;

    if (!reader)
        return XWA_REMASTER_ORIGINAL_2D_LOAD_FAILED;

    status = XwaRemasterOriginal2d_LoadDatGroup(
        reader,
        TEST_GROUP_ID,
        frames,
        error,
        error_size);

    XwaRemasterOriginal2d_Destroy(reader);
    return status;
}

static int frame_is_rgba(
    const Xwa2dFrameSet* frames,
    uint8_t r,
    uint8_t g,
    uint8_t b,
    uint8_t a) {
    if (!frames ||
        frames->count != 1 ||
        !frames->frames ||
        !frames->frames[0].rgba ||
        frames->frames[0].width != 1 ||
        frames->frames[0].height != 1)
        return 0;

    const uint8_t* pixel = frames->frames[0].rgba;
    return
        pixel[0] == r &&
        pixel[1] == g &&
        pixel[2] == b &&
        pixel[3] == a;
}

static int test_hd_preferred(void) {
    static const uint8_t red_bgra[4] = { 0, 0, 255, 255 };
    static const uint8_t green_bgra[4] = { 0, 255, 0, 255 };
    uint8_t original_dat[124];
    uint8_t hd_dat[124];
    Xwa2dFrameSet frames = { 0 };
    char error[256] = { 0 };
    int ok = 0;

    if (!prepare_root())
        goto done;

    build_dat(original_dat, red_bgra);
    build_dat(hd_dat, green_bgra);
    if (!write_bytes(TEST_ROOT, "TEST.DAT", original_dat, sizeof original_dat) ||
        !write_bytes(TEST_ROOT, "TEST_HD.DAT", hd_dat, sizeof hd_dat))
        goto done;

    if (load_group(&frames, error, sizeof error) !=
        XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS) {
        fprintf(stderr, "FAIL hd-preferred: load failed: %s\n", error);
        goto done;
    }

    if (!frame_is_rgba(&frames, 0, 255, 0, 255)) {
        fprintf(
            stderr,
            "FAIL hd-preferred: expected TEST_HD.DAT (green), "
            "but original DAT won\n");
        goto done;
    }

    ok = 1;

done:
    Xwa2dFrameSet_Free(&frames);
    clean_test_root();
    return ok;
}

static int test_original_fallback(void) {
    static const uint8_t red_bgra[4] = { 0, 0, 255, 255 };
    uint8_t original_dat[124];
    Xwa2dFrameSet frames = { 0 };
    char error[256] = { 0 };
    int ok = 0;

    if (!prepare_root())
        goto done;

    build_dat(original_dat, red_bgra);
    if (!write_bytes(TEST_ROOT, "TEST.DAT", original_dat, sizeof original_dat))
        goto done;

    if (load_group(&frames, error, sizeof error) !=
        XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS) {
        fprintf(stderr, "FAIL original-fallback: load failed: %s\n", error);
        goto done;
    }

    if (!frame_is_rgba(&frames, 255, 0, 0, 255)) {
        fprintf(stderr, "FAIL original-fallback: original DAT was not used\n");
        goto done;
    }

    ok = 1;

done:
    Xwa2dFrameSet_Free(&frames);
    clean_test_root();
    return ok;
}

static int test_corrupt_hd_does_not_fallback(void) {
    static const uint8_t red_bgra[4] = { 0, 0, 255, 255 };
    static const uint8_t corrupt_hd[] = { 'b', 'a', 'd' };
    uint8_t original_dat[124];
    Xwa2dFrameSet frames = { 0 };
    char error[256] = { 0 };
    XwaRemasterOriginal2dLoadStatus status;
    int ok = 0;

    if (!prepare_root())
        goto done;

    build_dat(original_dat, red_bgra);
    if (!write_bytes(TEST_ROOT, "TEST.DAT", original_dat, sizeof original_dat) ||
        !write_bytes(TEST_ROOT, "TEST_HD.DAT", corrupt_hd, sizeof corrupt_hd))
        goto done;

    status = load_group(&frames, error, sizeof error);
    if (status == XWA_REMASTER_ORIGINAL_2D_LOAD_SUCCESS) {
        fprintf(
            stderr,
            "FAIL corrupt-hd: corrupt TEST_HD.DAT was silently hidden "
            "by fallback to TEST.DAT\n");
        goto done;
    }

    ok = 1;

done:
    Xwa2dFrameSet_Free(&frames);
    clean_test_root();
    return ok;
}

int main(void) {
    int failures = 0;

    if (!test_hd_preferred())
        ++failures;
    else
        printf("PASS: *_HD.dat preferred over listed DAT\n");

    if (!test_original_fallback())
        ++failures;
    else
        printf("PASS: listed DAT used when *_HD.dat is absent\n");

    if (!test_corrupt_hd_does_not_fallback())
        ++failures;
    else
        printf("PASS: corrupt *_HD.dat is not silently hidden\n");

    if (failures != 0) {
        fprintf(
            stderr,
            "RED as expected before XWAU-003 implementation: %d failure(s)\n",
            failures);
        return 1;
    }

    printf("PASS: XWAU HD DAT resolution semantics\n");
    return 0;
}
