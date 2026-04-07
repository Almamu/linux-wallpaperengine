#pragma once

#include <memory>

#include "frontends/context.h"

#include "Configuration.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Model/Wallpaper.h"

#include "Audio/AudioContext.h"
#include "Render/RenderContext.h"

namespace WallpaperEngine {
class Context {
public:
	std::unique_ptr<TextureCache> texture_cache;
	std::unique_ptr<Audio::AudioContext> audio;
	const Configuration* config;
	wp_gl_proc_address* gl_proc_address;
	wp_time_counter* time_counter;
	wp_audio_input_mix* audio_input_mix;
	wp_mute_check* mute_check;
	std::vector<Data::Model::ProjectUniquePtr> projects;
	bool isRunning;
	float renderTime;
	float renderTimeLast;
	float daytime;

	explicit Context (
		const Configuration* config, wp_time_counter* time_counter, wp_gl_proc_address* gl_proc_addresses,
		wp_audio_input_mix* audio_input_mix
	);

	void updateTime ();
};
}