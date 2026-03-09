#pragma once

#include "../../include/frontends/configuration.h"

#include <filesystem>
#include <map>
#include <string>

namespace WallpaperEngine {
struct Configuration {
	std::map<std::string, std::string> properties;
	wp_rendering_pause_check* pause_check;
	wp_mute_check* mute_check;
	int volume;
	bool enableAudio;
	bool disableParticles;
	bool disableParallax;
	int web_fps;

	std::filesystem::path assets_dir;
	std::filesystem::path backgrounds_dir;
};
}