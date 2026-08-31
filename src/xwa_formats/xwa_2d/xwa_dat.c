#include "xwa_2d_internal.h"

#include "LzmaDec.h"

static void* dat_lzma_alloc(ISzAllocPtr allocator, size_t size) {
    (void)allocator;
    return malloc(size);
}

static void dat_lzma_free(ISzAllocPtr allocator, void* address) {
    (void)allocator;
    free(address);
}

static const ISzAlloc dat_lzma_allocator = {
    dat_lzma_alloc,
    dat_lzma_free
};

static uint8_t dat_expand_5(uint16_t value) {
    return (uint8_t)((value << 3) | (value >> 2));
}

static uint8_t dat_expand_6(uint16_t value) {
    return (uint8_t)((value << 2) | (value >> 4));
}

static void dat_decode_bc3_block(const uint8_t* block, uint8_t rgba[64]) {
    uint8_t alphas[8];

    alphas[0] = block[0];
    alphas[1] = block[1];

    if (alphas[0] > alphas[1]) {
        for (int i = 1; i <= 6; i++)
            alphas[i + 1] =
                (uint8_t)(((7 - i) * alphas[0] + i * alphas[1]) / 7);
    } else {
        for (int i = 1; i <= 4; i++)
            alphas[i + 1] =
                (uint8_t)(((5 - i) * alphas[0] + i * alphas[1]) / 5);

        alphas[6] = 0;
        alphas[7] = 255;
    }

    uint64_t alpha_indices = 0;

    for (int i = 0; i < 6; i++)
        alpha_indices |= (uint64_t)block[2 + i] << (8 * i);

    uint8_t colors[4][3];

    const uint16_t color0 = xwa2d_u16(block + 8);
    const uint16_t color1 = xwa2d_u16(block + 10);

    colors[0][0] = dat_expand_5((uint16_t)(color0 >> 11));
    colors[0][1] = dat_expand_6((uint16_t)((color0 >> 5) & 0x3f));
    colors[0][2] = dat_expand_5((uint16_t)(color0 & 0x1f));

    colors[1][0] = dat_expand_5((uint16_t)(color1 >> 11));
    colors[1][1] = dat_expand_6((uint16_t)((color1 >> 5) & 0x3f));
    colors[1][2] = dat_expand_5((uint16_t)(color1 & 0x1f));

    for (int channel = 0; channel < 3; channel++) {
        colors[2][channel] =
            (uint8_t)((2 * colors[0][channel] + colors[1][channel]) / 3);

        colors[3][channel] =
            (uint8_t)((colors[0][channel] + 2 * colors[1][channel]) / 3);
    }

    const uint32_t color_indices = xwa2d_u32(block + 12);

    for (int i = 0; i < 16; i++) {
        const uint8_t color_index =
            (uint8_t)((color_indices >> (2 * i)) & 3u);

        const uint8_t alpha_index =
            (uint8_t)((alpha_indices >> (3 * i)) & 7u);

        rgba[4 * i] = colors[color_index][0];
        rgba[4 * i + 1] = colors[color_index][1];
        rgba[4 * i + 2] = colors[color_index][2];
        rgba[4 * i + 3] = alphas[alpha_index];
    }
}

static int dat_decode_bc3(
    const uint8_t* blocks,
    size_t encoded_size,
    int width,
    int height,
    uint8_t* rgba) {

    const size_t block_width = ((size_t)width + 3u) / 4u;
    const size_t block_height = ((size_t)height + 3u) / 4u;

    if (block_width > SIZE_MAX / block_height)
        return 0;

    const size_t block_count = block_width * block_height;

    if (block_count > encoded_size / 16u ||
        encoded_size != block_count * 16u)
        return 0;

    for (size_t block_y = 0; block_y < block_height; block_y++) {
        for (size_t block_x = 0; block_x < block_width; block_x++) {
            uint8_t decoded[64];

            const uint8_t* block =
                blocks + (block_y * block_width + block_x) * 16u;

            dat_decode_bc3_block(block, decoded);

            for (size_t y = 0;
                 y < 4 && block_y * 4u + y < (size_t)height;
                 y++) {

                for (size_t x = 0;
                     x < 4 && block_x * 4u + x < (size_t)width;
                     x++) {

                    memcpy(
                        rgba +
                            ((block_y * 4u + y) * (size_t)width +
                             block_x * 4u + x) *
                                4u,
                        decoded + (y * 4u + x) * 4u,
                        4u);
                }
            }
        }
    }

    return 1;
}

static void dat_decode_bc4_block(
    const uint8_t* block,
    uint8_t values[16]) {

    uint8_t palette[8];

    palette[0] = block[0];
    palette[1] = block[1];

    if (palette[0] > palette[1]) {
        for (int i = 1; i <= 6; i++)
            palette[i + 1] =
                (uint8_t)(((7 - i) * palette[0] +
                           i * palette[1]) / 7);
    } else {
        for (int i = 1; i <= 4; i++)
            palette[i + 1] =
                (uint8_t)(((5 - i) * palette[0] +
                           i * palette[1]) / 5);

        palette[6] = 0;
        palette[7] = 255;
    }

    uint64_t indices = 0;

    for (int i = 0; i < 6; i++)
        indices |= (uint64_t)block[2 + i] << (8 * i);

    for (int i = 0; i < 16; i++)
        values[i] =
            palette[(indices >> (3 * i)) & 7u];
}

static void dat_decode_bc5_block(
    const uint8_t* block,
    uint8_t rgba[64]) {

    uint8_t red[16];
    uint8_t green[16];

    dat_decode_bc4_block(block, red);
    dat_decode_bc4_block(block + 8, green);

    for (int i = 0; i < 16; i++) {
        rgba[4 * i] = red[i];
        rgba[4 * i + 1] = green[i];
        rgba[4 * i + 2] = 0;
        rgba[4 * i + 3] = 255;
    }
}

static int dat_decode_bc5(
    const uint8_t* blocks,
    size_t encoded_size,
    int width,
    int height,
    uint8_t* rgba) {

    const size_t block_width =
        ((size_t)width + 3u) / 4u;

    const size_t block_height =
        ((size_t)height + 3u) / 4u;

    if (block_width > SIZE_MAX / block_height)
        return 0;

    const size_t block_count =
        block_width * block_height;

    if (block_count > encoded_size / 16u ||
        encoded_size != block_count * 16u)
        return 0;

    for (size_t block_y = 0;
         block_y < block_height;
         block_y++) {

        for (size_t block_x = 0;
             block_x < block_width;
             block_x++) {

            uint8_t decoded[64];

            const uint8_t* block =
                blocks +
                (block_y * block_width + block_x) * 16u;

            dat_decode_bc5_block(block, decoded);

            for (size_t y = 0;
                 y < 4 &&
                 block_y * 4u + y < (size_t)height;
                 y++) {

                for (size_t x = 0;
                     x < 4 &&
                     block_x * 4u + x < (size_t)width;
                     x++) {

                    memcpy(
                        rgba +
                            ((block_y * 4u + y) *
                                 (size_t)width +
                             block_x * 4u + x) *
                                4u,
                        decoded +
                            (y * 4u + x) * 4u,
                        4u);
                }
            }
        }
    }

    return 1;
}

typedef struct DatEntry {
	uint16_t group;
	uint16_t sprite_count;
	uint32_t data_offset;
} DatEntry;

static int dat_directory(const uint8_t* bytes, size_t size, size_t* out_record_size,
						 uint16_t* out_entry_count, const uint8_t** out_entries,
						 char* error, size_t error_size) {
	if (!bytes || size < 10)
		return xwa2d_fail(error, error_size, "invalid DAT input");
	const size_t record_size = xwa2d_u16(bytes + 8) == 0 ? 12u : 24u;
	if (size < 10u + record_size)
		return xwa2d_fail(error, error_size, "truncated DAT directory");
	const uint16_t entry_count = xwa2d_u16(bytes + 10);
	if (entry_count > 512 || (size_t)(entry_count + 1) * record_size > size - 10u)
		return xwa2d_fail(error, error_size, "invalid DAT directory count");
	*out_record_size = record_size;
	*out_entry_count = entry_count;
	*out_entries = bytes + 10u + record_size;
	return 1;
}

int Xwa2d_DatListGroups(const uint8_t* bytes, size_t size, uint16_t* groups, int capacity,
						int* in_out_count, char* error, size_t error_size) {
	if (!groups || !in_out_count || *in_out_count < 0 || *in_out_count > capacity)
		return xwa2d_fail(error, error_size, "invalid DAT group output");
	size_t record_size;
	uint16_t entry_count;
	const uint8_t* entries;
	if (!dat_directory(bytes, size, &record_size, &entry_count, &entries, error, error_size))
		return 0;
	for (uint16_t i = 0; i < entry_count; i++) {
		const uint16_t group = xwa2d_u16(entries + (size_t)i * record_size);
		int found = 0;
		for (int j = 0; j < *in_out_count; j++) {
			if (groups[j] == group) {
				found = 1;
				break;
			}
		}
		if (!found) {
			if (*in_out_count >= capacity)
				return xwa2d_fail(error, error_size, "DAT group output is full");
			groups[(*in_out_count)++] = group;
		}
	}
	return 1;
}

static int dat_insert_frame(Xwa2dFrameSet* set, Xwa2dFrame* frame) {
	int insert = 0;
	while (insert < set->count && set->frames[insert].sprite_id < frame->sprite_id)
		insert++;
	if (insert < set->count && set->frames[insert].sprite_id == frame->sprite_id) {
		free(frame->rgba);
		return 1;
	}
	Xwa2dFrame* grown = (Xwa2dFrame*)realloc(set->frames, (size_t)(set->count + 1) * sizeof *grown);
	if (!grown)
		return 0;
	set->frames = grown;
	memmove(&set->frames[insert + 1], &set->frames[insert],
			(size_t)(set->count - insert) * sizeof *set->frames);
	set->frames[insert] = *frame;
	set->count++;
	for (int i = 0; i < set->count; i++)
		set->frames[i].frame_index = i;
	return 1;
}

int Xwa2d_DecodeDatSprite(const uint8_t* record, size_t record_size, Xwa2dFrame* out, char* error,
						  size_t error_size) {
	if (!record || !out || record_size < 18)
		return xwa2d_fail(error, error_size, "invalid DAT sprite input");
	memset(out, 0, sizeof *out);
	uint16_t type = xwa2d_u16(record);
	int width = xwa2d_u16(record + 2);
	int height = xwa2d_u16(record + 4);
	uint32_t payload_size = xwa2d_u32(record + 14);
	if (width <= 0 || height <= 0 || payload_size > record_size - 18 || payload_size < 44)
		return xwa2d_fail(error, error_size, "invalid DAT sprite header");
	const uint8_t* payload = record + 18;
	uint32_t color_offset = xwa2d_u32(payload + 4);
	uint32_t rows_offset = xwa2d_u32(payload + 8);
	uint32_t color_count = xwa2d_u32(payload + 40);

    if (type == 25 && color_count == 0) {
        if (rows_offset >= payload_size)
            return xwa2d_fail(error, error_size, "invalid DAT sprite payload");

        const size_t pixel_count = (size_t)width * (size_t)height;

        if (pixel_count > SIZE_MAX / 4u ||
            (size_t)(payload_size - rows_offset) != pixel_count * 4u)
            return xwa2d_fail(error, error_size, "invalid DAT sprite payload");

        const uint8_t* bgra = payload + rows_offset;
        uint8_t* rgba = (uint8_t*)malloc(pixel_count * 4u);

        if (!rgba)
            return xwa2d_fail(error, error_size, "DAT sprite allocation failed");

        for (size_t i = 0; i < pixel_count; i++) {
            rgba[4 * i] = bgra[4 * i + 2];
            rgba[4 * i + 1] = bgra[4 * i + 1];
            rgba[4 * i + 2] = bgra[4 * i];
            rgba[4 * i + 3] = bgra[4 * i + 3];
        }

        out->rgba = rgba;
        out->width = width;
        out->height = height;
        out->sprite_id = xwa2d_u16(record + 12);
        out->anchor_x = xwa2d_i32(payload + 24);
        out->anchor_y = xwa2d_i32(payload + 28);

        return 1;
    }

    if (type == 25 && color_count == 1) {
        if (rows_offset >= payload_size)
            return xwa2d_fail(error, error_size, "invalid DAT sprite payload");

        const size_t pixel_count = (size_t)width * (size_t)height;

        if (pixel_count > SIZE_MAX / 4u)
            return xwa2d_fail(error, error_size, "DAT sprite dimensions are too large");

        const size_t bgra_size = pixel_count * 4u;
        const uint8_t* encoded = payload + rows_offset;
        const size_t encoded_size = payload_size - rows_offset;

        if (encoded_size <= LZMA_PROPS_SIZE)
            return xwa2d_fail(error, error_size, "invalid DAT LZMA payload");

        uint8_t* rgba = (uint8_t*)malloc(bgra_size);

        if (!rgba)
            return xwa2d_fail(error, error_size, "DAT sprite allocation failed");

        SizeT decoded_size = (SizeT)bgra_size;
        SizeT compressed_size = (SizeT)(encoded_size - LZMA_PROPS_SIZE);
        ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;

        const SRes result = LzmaDecode(
            rgba,
            &decoded_size,
            encoded + LZMA_PROPS_SIZE,
            &compressed_size,
            encoded,
            LZMA_PROPS_SIZE,
            LZMA_FINISH_END,
            &status,
            &dat_lzma_allocator);

        if (result != SZ_OK ||
            decoded_size != bgra_size ||
            (status != LZMA_STATUS_FINISHED_WITH_MARK &&
             status != LZMA_STATUS_MAYBE_FINISHED_WITHOUT_MARK)) {
            free(rgba);
            return xwa2d_fail(error, error_size, "invalid DAT LZMA payload");
        }

        for (size_t i = 0; i < pixel_count; i++) {
            const uint8_t blue = rgba[4 * i];
            rgba[4 * i] = rgba[4 * i + 2];
            rgba[4 * i + 2] = blue;
        }

        out->rgba = rgba;
        out->width = width;
        out->height = height;
        out->sprite_id = xwa2d_u16(record + 12);
        out->anchor_x = xwa2d_i32(payload + 24);
        out->anchor_y = xwa2d_i32(payload + 28);

        return 1;
    }

    if (type == 25 && color_count == 2) {
        if (rows_offset >= payload_size)
            return xwa2d_fail(
                error,
                error_size,
                "invalid DAT BC3 payload");

        const size_t pixel_count =
            (size_t)width * (size_t)height;

        if (pixel_count > SIZE_MAX / 4u)
            return xwa2d_fail(
                error,
                error_size,
                "DAT sprite dimensions are too large");

        uint8_t* rgba =
            (uint8_t*)malloc(pixel_count * 4u);

        if (!rgba)
            return xwa2d_fail(
                error,
                error_size,
                "DAT sprite allocation failed");

        if (!dat_decode_bc3(
                payload + rows_offset,
                payload_size - rows_offset,
                width,
                height,
                rgba)) {

            free(rgba);

            return xwa2d_fail(
                error,
                error_size,
                "invalid DAT BC3 payload");
        }

        out->rgba = rgba;
        out->width = width;
        out->height = height;
        out->sprite_id = xwa2d_u16(record + 12);
        out->anchor_x = xwa2d_i32(payload + 24);
        out->anchor_y = xwa2d_i32(payload + 28);

        return 1;
    }

    if (type == 25 && color_count == 3) {
        if (rows_offset >= payload_size)
            return xwa2d_fail(
                error,
                error_size,
                "invalid DAT BC5 payload");

        const size_t pixel_count =
            (size_t)width * (size_t)height;

        if (pixel_count > SIZE_MAX / 4u)
            return xwa2d_fail(
                error,
                error_size,
                "DAT sprite dimensions are too large");

        uint8_t* rgba =
            (uint8_t*)malloc(pixel_count * 4u);

        if (!rgba)
            return xwa2d_fail(
                error,
                error_size,
                "DAT sprite allocation failed");

        if (!dat_decode_bc5(
                payload + rows_offset,
                payload_size - rows_offset,
                width,
                height,
                rgba)) {

            free(rgba);

            return xwa2d_fail(
                error,
                error_size,
                "invalid DAT BC5 payload");
        }

        out->rgba = rgba;
        out->width = width;
        out->height = height;
        out->sprite_id = xwa2d_u16(record + 12);
        out->anchor_x = xwa2d_i32(payload + 24);
        out->anchor_y = xwa2d_i32(payload + 28);

        return 1;
    }

	if (!color_count || color_count > 256 || color_offset > payload_size || rows_offset >= payload_size ||
		3u * color_count > payload_size - color_offset)
		return xwa2d_fail(error, error_size, "invalid DAT sprite payload");
	const uint8_t* colors = payload + color_offset;
	const uint8_t* p = payload + rows_offset;
	const uint8_t* end = payload + payload_size;
	uint8_t* rgba = (uint8_t*)calloc((size_t)width * height, 4);
	if (!rgba)
		return xwa2d_fail(error, error_size, "DAT sprite allocation failed");

	if (type == 24 || type == 25) {
		if ((size_t)(end - p) < (size_t)width * height * 2u)
			goto malformed;
		for (size_t i = 0; i < (size_t)width * height; i++) {
			uint8_t index = p[2 * i];
			uint8_t alpha = p[2 * i + 1];
			if (index >= color_count || !alpha)
				continue;
			rgba[4 * i] = colors[3u * index];
			rgba[4 * i + 1] = colors[3u * index + 1];
			rgba[4 * i + 2] = colors[3u * index + 2];
			rgba[4 * i + 3] = alpha;
		}
	} else {
		for (int y = 0; y < height; y++) {
			if (p >= end)
				goto malformed;
			int run_count = *p++;
			int x = 0;
			while (run_count-- > 0) {
				if (p >= end)
					goto malformed;
				uint8_t token = *p++;
				int length;
				int mode;
				if (type == 7) {
					length = token & 0x7f;
					mode = token & 0x80 ? 0xc0 : 0;
				} else {
					length = token & 0x3f;
					mode = token & 0xc0;
				}
				if (mode == 0xc0) {
					x += length;
					continue;
				}
				int bytes_per_pixel = mode == 0 ? 1 : 2;
				if ((size_t)(end - p) < (size_t)length * bytes_per_pixel)
					goto malformed;
				for (int i = 0; i < length; i++) {
					uint8_t alpha_code = mode == 0 ? 0xff : p[2 * i];
					uint8_t index = p[bytes_per_pixel * i + bytes_per_pixel - 1];
					if (x + i >= width || index >= color_count || alpha_code < 0x20)
						continue;
					uint8_t* out = rgba + ((size_t)y * width + x + i) * 4;
					out[0] = colors[3u * index];
					out[1] = colors[3u * index + 1];
					out[2] = colors[3u * index + 2];
					out[3] = alpha_code == 0xff ? 0xff : (uint8_t)(((alpha_code >> 5) * 255u) / 8u);
				}
				p += (size_t)length * bytes_per_pixel;
				x += length;
			}
		}
	}
	out->rgba = rgba;
	out->width = width;
	out->height = height;
	out->sprite_id = xwa2d_u16(record + 12);
	out->anchor_x = xwa2d_i32(payload + 24);
	out->anchor_y = xwa2d_i32(payload + 28);
	return 1;

malformed:
	free(rgba);
	return xwa2d_fail(error, error_size, "malformed DAT sprite pixels");
}

int Xwa2d_DatAppendGroup(const uint8_t* bytes, size_t size, uint16_t group, Xwa2dFrameSet* in_out,
						 char* error, size_t error_size) {
	if (!in_out)
		return xwa2d_fail(error, error_size, "invalid DAT group output");
	size_t record_size;
	uint16_t entry_count;
	const uint8_t* entries;
	if (!dat_directory(bytes, size, &record_size, &entry_count, &entries, error, error_size))
		return 0;
	DatEntry match = { 0 };
	int found = 0;
	for (uint16_t i = 0; i < entry_count; i++) {
		const uint8_t* entry = entries + (size_t)i * record_size;
		if (xwa2d_u16(entry) != group)
			continue;
		match.group = group;
		match.sprite_count = xwa2d_u16(entry + 2);
		match.data_offset = xwa2d_u32(entry + record_size - 4);
		found = 1;
		break;
	}
	if (!found)
		return 1;
	size_t cursor = 10u + (size_t)(entry_count + 1) * record_size;
	if (match.data_offset > size - cursor)
		return xwa2d_fail(error, error_size, "invalid DAT group offset");
	cursor += match.data_offset;
	for (uint16_t i = 0; i < match.sprite_count; i++) {
		if (size - cursor < 18)
			return xwa2d_fail(error, error_size, "truncated DAT sprite header");
		const uint8_t* record = bytes + cursor;
		uint32_t payload_size = xwa2d_u32(record + 14);
		if (payload_size > size - cursor - 18u)
			return xwa2d_fail(error, error_size, "truncated DAT sprite");
		cursor += 18u + payload_size;
		if (xwa2d_u16(record + 10) != group)
			continue;
		Xwa2dFrame frame = { 0 };
		if (!Xwa2d_DecodeDatSprite(record, 18u + payload_size, &frame, error, error_size))
			return 0;
		if (!dat_insert_frame(in_out, &frame)) {
			free(frame.rgba);
			return xwa2d_fail(error, error_size, "DAT allocation failed");
		}
	}
	return 1;
}
