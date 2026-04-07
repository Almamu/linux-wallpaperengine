#include "Context.h"

#include "Audio/Drivers/SDLAudioDriver.h"
#include "WallpaperEngine/Audio/AudioPlayingDetector.h"
#include "WallpaperEngine/Audio/Drivers/AudioDriver.h"
#include "WallpaperEngine/Audio/PlaybackRecorder.h"

WallpaperEngine::Context::Context (
	const Configuration* config, wp_time_counter* time_counter, wp_gl_proc_address* gl_proc_addresses,
	wp_audio_input_mix* audio_input_mix
) :
	texture_cache (std::make_unique<TextureCache> ()), config (config), gl_proc_address (gl_proc_addresses),
	time_counter (time_counter), audio_input_mix (audio_input_mix), isRunning (true), renderTime (0.0f),
	renderTimeLast (0.0f), daytime (0.0f) {
	this->audio = std::make_unique<Audio::AudioContext> (
		std::make_unique<Audio::Drivers::SDLAudioDriver> (*this), std::make_unique<Audio::PlaybackRecorder> (*this),
		std::make_unique<Audio::AudioPlayingDetector> (*this)
	);
}

void WallpaperEngine::Context::updateTime () {
	this->renderTimeLast = this->renderTime;
	this->renderTime = this->time_counter->get_time (this->time_counter->user_parameter);
}