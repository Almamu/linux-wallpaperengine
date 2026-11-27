#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Returns owned, top-down RGBA pixels or NULL if this JPEG is not eligible. */
unsigned char* lwe_decode_tex_jpeg (const unsigned char* bytes, int size, int* width, int* height);

#ifdef __cplusplus
}
#endif
