#include "xwa_2d_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(void) {
    enum {
        payload_header_size = 44,
        pixel_bytes = 8,
        payload_size = payload_header_size + pixel_bytes,
        record_size = 18 + payload_size
    };

    uint8_t record[record_size];
    memset(record, 0, sizeof record);

    put_u16(record + 0, 25);
    put_u16(record + 2, 2);
    put_u16(record + 4, 1);
    put_u16(record + 10, 42);
    put_u16(record + 12, 7);
    put_u32(record + 14, payload_size);

    uint8_t* payload = record + 18;
    put_u32(payload + 8, payload_header_size);
    put_u32(payload + 40, 0);

    uint8_t* pixels = payload + payload_header_size;

    pixels[0] = 30;
    pixels[1] = 20;
    pixels[2] = 10;
    pixels[3] = 40;

    pixels[4] = 3;
    pixels[5] = 2;
    pixels[6] = 1;
    pixels[7] = 4;

    const uint8_t expected[] = {
        10, 20, 30, 40,
        1, 2, 3, 4
    };

    Xwa2dFrame frame;
    char error[256] = { 0 };

    if (!Xwa2d_DecodeDatSprite(
            record, sizeof record, &frame, error, sizeof error)) {
        fprintf(stderr, "FAIL: OpenXWA rechaza DAT type-25 BGRA: %s\n", error);
        return 1;
    }

    if (frame.width != 2 ||
        frame.height != 1 ||
        !frame.rgba ||
        memcmp(frame.rgba, expected, sizeof expected) != 0) {
        fprintf(stderr, "FAIL: salida RGBA incorrecta\n");
        free(frame.rgba);
        return 1;
    }

    free(frame.rgba);

    printf("PASS: DAT type-25 BGRA decodificado correctamente\n");
    return 0;
}