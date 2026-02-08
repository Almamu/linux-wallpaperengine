#pragma once

#if DEMOMODE

#include <cstdint>
#include <cstdlib>
#include <vector>

extern const int FPS;
extern const int FRAME_COUNT;

int init_encoder (const char* output_file, int sourceWidth, int sourceHeight);
int write_video_frame (const uint8_t* rgb_data);
int close_encoder ();

#endif /* DEMOMODE */