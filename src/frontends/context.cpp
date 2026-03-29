#include "frontends/context.h"

#include "WallpaperEngine/Audio/Drivers/Detectors/AudioPlayingDetector.h"
#include "WallpaperEngine/Audio/Drivers/SDLAudioDriver.h"
#include "WallpaperEngine/Context.h"

float default_get_time (void* user_parameter) { return SDL_GetTicks () / 1000.0f; }

wp_time_counter default_time_counter = { .user_parameter = nullptr, .get_time = default_get_time };

#define WPENGINE_CONTEXT_API_BEGIN try {
#define WPENGINE_CONTEXT_API_END(result)                                                                               \
	}                                                                                                                  \
	catch (...) {                                                                                                      \
		return result;                                                                                                 \
	}
WPENGINE_API wp_context* wp_context_create (const wp_configuration* config) {
	WPENGINE_CONTEXT_API_BEGIN
	return new WallpaperEngine::Context (
		static_cast<const WallpaperEngine::Configuration*> (config), &default_time_counter, nullptr
	);
	WPENGINE_CONTEXT_API_END (nullptr)
}

WPENGINE_API void wp_context_destroy (wp_context* context) {
	WPENGINE_CONTEXT_API_BEGIN
	delete static_cast<WallpaperEngine::Context*> (context);
	WPENGINE_CONTEXT_API_END ();
}

WPENGINE_API void wp_context_set_gl_proc_address (wp_context* context, wp_gl_proc_address* address) {
	WPENGINE_CONTEXT_API_BEGIN
	static_cast<WallpaperEngine::Context*> (context)->gl_proc_address = address ?: nullptr;
	WPENGINE_CONTEXT_API_END ();
}

WPENGINE_API void wp_context_set_time_counter (wp_context* context, wp_time_counter* counter) {
	WPENGINE_CONTEXT_API_BEGIN
	static_cast<WallpaperEngine::Context*> (context)->time_counter = counter ?: &default_time_counter;
	WPENGINE_CONTEXT_API_END ();
}
