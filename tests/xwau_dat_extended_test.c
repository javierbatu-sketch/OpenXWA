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

    {
        static const uint8_t lzma_data[] = {
            0x5d, 0x00, 0x00, 0x10, 0x00,
            0x00, 0x0f, 0x05, 0x55, 0xf5, 0x25, 0xb9,
            0x0b, 0x5e, 0x40, 0x92, 0xf8, 0xf0, 0xff,
            0xff, 0x09, 0x20, 0x00, 0x00
        };

        enum {
            lzma_header_size = 44,
            lzma_payload_size = lzma_header_size + sizeof lzma_data,
            lzma_record_size = 18 + lzma_payload_size
        };

        uint8_t lzma_record[lzma_record_size];
        memset(lzma_record, 0, sizeof lzma_record);

        put_u16(lzma_record + 0, 25);
        put_u16(lzma_record + 2, 2);
        put_u16(lzma_record + 4, 1);
        put_u16(lzma_record + 10, 42);
        put_u16(lzma_record + 12, 8);
        put_u32(lzma_record + 14, lzma_payload_size);

        uint8_t* lzma_payload = lzma_record + 18;

        put_u32(lzma_payload + 8, lzma_header_size);
        put_u32(lzma_payload + 40, 1);

        memcpy(
            lzma_payload + lzma_header_size,
            lzma_data,
            sizeof lzma_data);

        const uint8_t lzma_expected[] = {
            10, 20, 30, 40,
            1, 2, 3, 4
        };

        Xwa2dFrame lzma_frame;
        char lzma_error[256] = { 0 };

        if (!Xwa2d_DecodeDatSprite(
                lzma_record,
                sizeof lzma_record,
                &lzma_frame,
                lzma_error,
                sizeof lzma_error)) {
            fprintf(
                stderr,
                "FAIL: OpenXWA rechaza DAT type-25 LZMA: %s\n",
                lzma_error);
            return 1;
        }

        if (!lzma_frame.rgba ||
            memcmp(
                lzma_frame.rgba,
                lzma_expected,
                sizeof lzma_expected) != 0) {
            fprintf(stderr, "FAIL: salida LZMA RGBA incorrecta\n");
            free(lzma_frame.rgba);
            return 1;
        }

        free(lzma_frame.rgba);

        printf("PASS: DAT type-25 LZMA decodificado correctamente\n");
    }


    {
        /*
         * Un bloque BC3 de 4x4.
         * Todos los p?xeles deben resultar rojo opaco.
         */
        static const uint8_t bc3_data[16] = {
            0xff, 0xff, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,

            0x00, 0xf8, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00
        };

        enum {
            bc3_header_size = 44,
            bc3_payload_size = bc3_header_size + sizeof bc3_data,
            bc3_record_size = 18 + bc3_payload_size
        };

        uint8_t bc3_record[bc3_record_size];
        memset(bc3_record, 0, sizeof bc3_record);

        put_u16(bc3_record + 0, 25);
        put_u16(bc3_record + 2, 4);
        put_u16(bc3_record + 4, 4);
        put_u16(bc3_record + 12, 9);
        put_u32(bc3_record + 14, bc3_payload_size);

        uint8_t* bc3_payload = bc3_record + 18;

        put_u32(bc3_payload + 8, bc3_header_size);

        /* XWAU: color_count = 2 significa BC3. */
        put_u32(bc3_payload + 40, 2);

        memcpy(
            bc3_payload + bc3_header_size,
            bc3_data,
            sizeof bc3_data);

        Xwa2dFrame bc3_frame;
        char bc3_error[256] = { 0 };

        if (!Xwa2d_DecodeDatSprite(
                bc3_record,
                sizeof bc3_record,
                &bc3_frame,
                bc3_error,
                sizeof bc3_error)) {
            fprintf(
                stderr,
                "FAIL: OpenXWA rechaza DAT type-25 BC3: %s\n",
                bc3_error);
            return 1;
        }

        if (!bc3_frame.rgba) {
            fprintf(stderr, "FAIL: BC3 no produjo pixels RGBA\n");
            return 1;
        }

        for (int i = 0; i < 16; i++) {
            const uint8_t* pixel = bc3_frame.rgba + i * 4;

            if (pixel[0] != 255 ||
                pixel[1] != 0 ||
                pixel[2] != 0 ||
                pixel[3] != 255) {
                fprintf(stderr, "FAIL: salida BC3 RGBA incorrecta\n");
                free(bc3_frame.rgba);
                return 1;
            }
        }

        free(bc3_frame.rgba);

        printf("PASS: DAT type-25 BC3 decodificado correctamente\n");
    }

    {
        /*
         * XWAU BC5:
         * primer bloque BC4 -> rojo = 64
         * segundo bloque BC4 -> verde = 192
         * salida esperada = (64, 192, 0, 255)
         */
        static const uint8_t bc5_data[16] = {
            64, 64, 0, 0, 0, 0, 0, 0,
            192, 192, 0, 0, 0, 0, 0, 0
        };

        enum {
            bc5_header_size = 44,
            bc5_payload_size = bc5_header_size + sizeof bc5_data,
            bc5_record_size = 18 + bc5_payload_size
        };

        uint8_t bc5_record[bc5_record_size];
        memset(bc5_record, 0, sizeof bc5_record);

        put_u16(bc5_record + 0, 25);
        put_u16(bc5_record + 2, 4);
        put_u16(bc5_record + 4, 4);
        put_u16(bc5_record + 12, 10);
        put_u32(bc5_record + 14, bc5_payload_size);

        uint8_t* bc5_payload = bc5_record + 18;

        put_u32(bc5_payload + 8, bc5_header_size);

        /* XWAU: color_count = 3 significa BC5. */
        put_u32(bc5_payload + 40, 3);

        memcpy(
            bc5_payload + bc5_header_size,
            bc5_data,
            sizeof bc5_data);

        Xwa2dFrame bc5_frame;
        char bc5_error[256] = { 0 };

        if (!Xwa2d_DecodeDatSprite(
                bc5_record,
                sizeof bc5_record,
                &bc5_frame,
                bc5_error,
                sizeof bc5_error)) {
            fprintf(
                stderr,
                "FAIL: OpenXWA rechaza DAT type-25 BC5: %s\n",
                bc5_error);
            return 1;
        }

        if (!bc5_frame.rgba) {
            fprintf(stderr, "FAIL: BC5 no produjo pixels RGBA\n");
            return 1;
        }

        for (int i = 0; i < 16; i++) {
            const uint8_t* pixel = bc5_frame.rgba + i * 4;

            if (pixel[0] != 64 ||
                pixel[1] != 192 ||
                pixel[2] != 0 ||
                pixel[3] != 255) {
                fprintf(stderr, "FAIL: salida BC5 RGBA incorrecta\n");
                free(bc5_frame.rgba);
                return 1;
            }
        }

        free(bc5_frame.rgba);

        printf("PASS: DAT type-25 BC5 decodificado correctamente\n");
    }

    return 0;
}