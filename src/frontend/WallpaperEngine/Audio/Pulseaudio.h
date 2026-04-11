#pragma once

#include "WallpaperEngine/Desktop/Environment.h"

#include <linux-wallpaperengine/context.h>
#include <pulse/pulseaudio.h>

namespace WallpaperEngine::Audio {
struct Pulseaudio {
	Pulseaudio ();
	~Pulseaudio ();

	wp_audio_input_mix input_mix;
	wp_mute_check mute_check;
	uint8_t* audioBuffer;
	uint8_t* audioBufferTmp;
	bool fullFrameReady;
	pa_stream* captureStream;
	size_t currentWritePointer;
	bool anythingPlaying;

	pa_mainloop* mainloop;
	pa_mainloop_api* mainloopApi;
	pa_context* context;

	Desktop::Environment* desktopEnvironment;
};
}