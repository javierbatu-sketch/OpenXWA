#include "xwa_2d.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void free_frame_set(Xwa2dFrameSet* set) {
    if (!set)
        return;

    for (int i = 0; i < set->count; i++)
        free(set->frames[i].rgba);

    free(set->frames);
    set->frames = NULL;
    set->count = 0;
}

static int read_dat_file(const char* path, uint8_t** out_bytes, size_t* out_size) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        printf("FAIL\tread\tcannot open DAT\n");
        return 0;
    }

#if defined(_WIN32)
    if (_fseeki64(file, 0, SEEK_END) != 0) {
        fclose(file);
        printf("FAIL\tread\tcannot seek DAT\n");
        return 0;
    }

    const __int64 end = _ftelli64(file);
    if (end < 0) {
        fclose(file);
        printf("FAIL\tread\tcannot determine DAT size\n");
        return 0;
    }

    if ((uint64_t)end > UINT32_MAX) {
        fclose(file);
        printf("FAIL\tread\tDAT exceeds UINT32_MAX read limit\n");
        return 0;
    }

    if (_fseeki64(file, 0, SEEK_SET) != 0) {
        fclose(file);
        printf("FAIL\tread\tcannot rewind DAT\n");
        return 0;
    }

    const size_t size = (size_t)end;
#else
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        printf("FAIL\tread\tcannot seek DAT\n");
        return 0;
    }

    const long end = ftell(file);
    if (end < 0 || (uint64_t)end > UINT32_MAX) {
        fclose(file);
        printf("FAIL\tread\tinvalid DAT size\n");
        return 0;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        printf("FAIL\tread\tcannot rewind DAT\n");
        return 0;
    }

    const size_t size = (size_t)end;
#endif

    uint8_t* bytes = (uint8_t*)malloc(size ? size : 1u);
    if (!bytes) {
        fclose(file);
        printf("FAIL\tread\tDAT allocation failed\n");
        return 0;
    }

    if (size && fread(bytes, 1, size, file) != size) {
        free(bytes);
        fclose(file);
        printf("FAIL\tread\tcannot read complete DAT\n");
        return 0;
    }

    fclose(file);
    *out_bytes = bytes;
    *out_size = size;
    return 1;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: xwau_dat_validator.exe <file.dat>\n");
        return 64;
    }

    uint8_t* bytes = NULL;
    size_t size = 0;

    if (!read_dat_file(argv[1], &bytes, &size))
        return 2;

    uint16_t groups[512];
    int group_count = 0;
    char error[512] = {0};

    if (!Xwa2d_DatListGroups(
            bytes,
            size,
            groups,
            (int)(sizeof groups / sizeof groups[0]),
            &group_count,
            error,
            sizeof error)) {
        printf("FAIL\tlist-groups\t%s\n", error[0] ? error : "unknown DAT error");
        free(bytes);
        return 2;
    }

    long long frame_total = 0;

    for (int i = 0; i < group_count; i++) {
        Xwa2dFrameSet set = {0};
        error[0] = '\0';

        if (!Xwa2d_DatAppendGroup(
                bytes,
                size,
                groups[i],
                &set,
                error,
                sizeof error)) {
            printf(
                "FAIL\tdecode\tgroup=%u\t%s\n",
                (unsigned)groups[i],
                error[0] ? error : "unknown DAT decode error");
            free_frame_set(&set);
            free(bytes);
            return 2;
        }

        frame_total += set.count;
        free_frame_set(&set);
    }

    free(bytes);
    printf("OK\t%d\t%lld\n", group_count, frame_total);
    return 0;
}
