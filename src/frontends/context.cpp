#include "frontends/context.h"

#include "WallpaperEngine/Audio/Drivers/Detectors/AudioPlayingDetector.h"
#include "WallpaperEngine/Audio/Drivers/SDLAudioDriver.h"
#include "WallpaperEngine/Context.h"

float default_get_time (void* user_parameter) { return SDL_GetTicks () / 1000.0f; }

wp_time_counter default_time_counter = { .user_parameter = nullptr, .get_time = default_get_time };

wp_context* wp_context_create (const wp_configuration* config) {
	auto configPtr = static_cast<const WallpaperEngine::Configuration*> (config);
	auto result = new WallpaperEngine::Context {
		.texture_cache = std::make_unique<TextureCache> (),
		.config = configPtr,
		.time_counter = nullptr,
		.projects = {},
		.isRunning = true,
		.renderTime = 0.0f,
		.renderTimeLast = 0.0f,
		.daytime = 0.0f,
	};

	result->audio = std::make_unique<WallpaperEngine::Audio::AudioContext> (
		std::make_unique<WallpaperEngine::Audio::Drivers::SDLAudioDriver> (
			*result,
			std::make_unique<WallpaperEngine::Audio::Drivers::Detectors::AudioPlayingDetector> (
				*configPtr->mute_check,
				std::make_unique<WallpaperEngine::Render::Drivers::Detectors::FullScreenDetector> (
					*configPtr->pause_check
				)
			),
			// TODO: SEPARATE THIS INTO ANOTHER HEADER, FOR NOW USE PULSEAUDIO DIRECTLY
			std::make_unique<WallpaperEngine::Audio::Drivers::Recorders::PulseAudioPlaybackRecorder> ()
		)
	);

	return result;
}

void wp_context_destroy (wp_context* context) { delete static_cast<WallpaperEngine::Context*> (context); }

void wp_context_set_gl_proc_address (wp_context* context, wp_gl_proc_address* address) {
	static_cast<WallpaperEngine::Context*> (context)->gl_proc_address = address;
}

void wp_context_set_time_counter (wp_context* context, wp_time_counter* counter) {
	static_cast<WallpaperEngine::Context*> (context)->time_counter = counter;
}
