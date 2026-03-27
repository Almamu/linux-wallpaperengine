#include "Context.h"

#include "Audio/Drivers/SDLAudioDriver.h"
#include "WallpaperEngine/Audio/Drivers/AudioDriver.h"

WallpaperEngine::Context::Context (const Configuration* config) :
	texture_cache (std::make_unique<TextureCache> ()), audio (nullptr), config (config), gl_proc_address (nullptr),
	time_counter (nullptr), projects (), isRunning (true), renderTime (0.0f), renderTimeLast (0.0f), daytime (0.0f) {
	this->audio = std::make_unique<WallpaperEngine::Audio::AudioContext> (
		std::make_unique<WallpaperEngine::Audio::Drivers::SDLAudioDriver> (
			*this,
			std::make_unique<WallpaperEngine::Audio::Drivers::Detectors::AudioPlayingDetector> (
				*this->config->mute_check,
				std::make_unique<WallpaperEngine::Render::Drivers::Detectors::FullScreenDetector> (
					*this->config->pause_check
				)
			),
			// TODO: SEPARATE THIS INTO ANOTHER HEADER, FOR NOW USE PULSEAUDIO DIRECTLY
			std::make_unique<WallpaperEngine::Audio::Drivers::Recorders::PulseAudioPlaybackRecorder> ()
		)
	);
}

void WallpaperEngine::Context::updateTime () {
	this->renderTimeLast = this->renderTime;
	this->renderTime = this->time_counter->get_time (this->time_counter->user_parameter);
}