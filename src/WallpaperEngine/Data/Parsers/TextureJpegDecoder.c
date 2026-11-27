#include "TextureJpegDecoder.h"

#include <limits.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <jpeglib.h>

struct Decoder {
    struct jpeg_decompress_struct jpeg;
    struct jpeg_error_mgr errors;
    jmp_buf failure;
    unsigned char* pixels;
};

static void jpeg_failed (j_common_ptr jpeg) {
    struct Decoder* decoder = jpeg->client_data;
    longjmp (decoder->failure, 1);
}

static void discard_jpeg_message (j_common_ptr jpeg) { (void) jpeg; }

static int decode (struct Decoder* decoder, const unsigned char* bytes, int size) {
    j_decompress_ptr jpeg = &decoder->jpeg;
    jpeg_create_decompress (jpeg);
    jpeg_mem_src (jpeg, bytes, (unsigned long) size);
    // Retain only marker presence. Metadata-bearing images use the legacy path.
    jpeg_save_markers (jpeg, JPEG_APP0 + 1, 1);
    jpeg_save_markers (jpeg, JPEG_APP0 + 2, 1);
    if (jpeg_read_header (jpeg, TRUE) != JPEG_HEADER_OK || !jpeg->is_baseline || !jpeg->saw_JFIF_marker
	|| jpeg->data_precision != 8 || jpeg->jpeg_color_space != JCS_YCbCr || jpeg->num_components != 3
	|| jpeg->saw_Adobe_marker || jpeg->marker_list) return 0;

    const jpeg_component_info* components = jpeg->comp_info;
    if (components[0].h_samp_factor != 2 || components[0].v_samp_factor != 2
	|| components[1].h_samp_factor != 1 || components[1].v_samp_factor != 1
	|| components[2].h_samp_factor != 1 || components[2].v_samp_factor != 1) return 0;

    // Match stb's signed-int allocation ceiling for decoded RGBA buffers.
    const uint64_t pixelCount = (uint64_t) jpeg->image_width * jpeg->image_height;
    if (pixelCount == 0 || pixelCount > INT_MAX / 4) return 0;
    jpeg->out_color_space = JCS_RGB;
    jpeg->dct_method = JDCT_ISLOW;
    jpeg->do_fancy_upsampling = TRUE;
    if (!jpeg_start_decompress (jpeg)) return 0;

    decoder->pixels = malloc ((size_t) pixelCount * 4);
    if (!decoder->pixels) return 0;
    JSAMPARRAY row = (*jpeg->mem->alloc_sarray) ((j_common_ptr) jpeg, JPOOL_IMAGE, jpeg->output_width * 3, 1);
    while (jpeg->output_scanline < jpeg->output_height) {
	unsigned char* pixel = decoder->pixels + (size_t) jpeg->output_scanline * jpeg->output_width * 4;
	if (jpeg_read_scanlines (jpeg, row, 1) != 1) return 0;
	const JSAMPLE* sample = row[0];
	for (JDIMENSION x = 0; x < jpeg->output_width; ++x, pixel += 4, sample += 3) {
	    pixel[0] = sample[0];
	    pixel[1] = sample[1];
	    pixel[2] = sample[2];
	    pixel[3] = 255;
	}
    }
    return jpeg_finish_decompress (jpeg) && decoder->errors.num_warnings == 0;
}

unsigned char* lwe_decode_tex_jpeg (const unsigned char* bytes, int size, int* width, int* height) {
    if (!bytes || size < 2 || bytes[0] != 0xff || bytes[1] != 0xd8) return NULL;
    // The decoder and allocation ownership live outside the setjmp frame's
    // automatic state, so error recovery can always release partial output.
    struct Decoder* decoder = calloc (1, sizeof (*decoder));
    if (!decoder) return NULL;
    decoder->jpeg.err = jpeg_std_error (&decoder->errors);
    decoder->jpeg.client_data = decoder;
    decoder->errors.error_exit = jpeg_failed;
    decoder->errors.output_message = discard_jpeg_message;

    int decoded;
    if (setjmp (decoder->failure) == 0) {
	decoded = decode (decoder, bytes, size);
    } else {
	decoded = 0;
    }

    unsigned char* pixels = NULL;
    if (decoded) {
	pixels = decoder->pixels;
	decoder->pixels = NULL;
	*width = (int) decoder->jpeg.output_width;
	*height = (int) decoder->jpeg.output_height;
    }
    jpeg_destroy_decompress (&decoder->jpeg);
    free (decoder->pixels);
    free (decoder);
    return pixels;
}
