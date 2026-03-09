#include <vector>

#include "../../include/frontends/configuration.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Configuration.h"
#include "WallpaperEngine/Logging/Log.h"

#define WORKSHOP_APP_ID 431960

bool wp_null_is_muted_or_paused (void* user_parameter);

wp_mute_check null_mute_check = {
	.user_parameter = nullptr,
	.is_muted = wp_null_is_muted_or_paused,
};

wp_rendering_pause_check null_rendering_pause_check = {
	.user_parameter = nullptr,
	.is_paused = wp_null_is_muted_or_paused,
};

bool wp_null_is_muted_or_paused (void* user_parameter) { return false; }

std::vector<std::string> steam_paths = {
	".steam/steam",
	".local/share/Steam",
	".var/app/com.valvesoftware.Steam/.local/share/Steam",
	"snap/steam/common/.local/share/Steam",
};

std::filesystem::path detectHomedir () {
	char* home = getenv ("HOME");

	if (home == nullptr) {
		sLog.exception ("Cannot find home directory for the current user");
	}

	const std::filesystem::path path = home;

	if (!std::filesystem::is_directory (path)) {
		sLog.exception ("Cannot find home directory for current user, ", home, " is not a directory");
	}

	return path;
}

wp_configuration* wp_config_create () {
	auto result = new WallpaperEngine::Configuration {
		.properties = {},
		.pause_check = &null_rendering_pause_check,
		.mute_check = &null_mute_check,
		.volume = 15,
		.enableAudio = true,
		.disableParticles = false,
		.disableParallax = false,
		.web_fps = 60,
	};

	wp_config_detect_steam_dir (result);

	return result;
}

void wp_config_destroy (wp_configuration* config) {
	if (config == nullptr) {
		return;
	}

	delete static_cast<WallpaperEngine::Configuration*> (config);
}

bool wp_config_set_assets_dir (wp_configuration* config, const char* dir) {
	const std::filesystem::path assets_dir = dir;

	if (!std::filesystem::exists (assets_dir) || !std::filesystem::is_directory (assets_dir)) {
		return false;
	}

	static_cast<WallpaperEngine::Configuration*> (config)->assets_dir = assets_dir;

	return true;
}

bool wp_config_set_backgrounds_dir (wp_configuration* config, const char* dir) {
	const std::filesystem::path backgrounds_dir = dir;

	if (!std::filesystem::exists (backgrounds_dir) || !std::filesystem::is_directory (backgrounds_dir)) {
		return false;
	}

	static_cast<WallpaperEngine::Configuration*> (config)->backgrounds_dir = backgrounds_dir;

	return true;
}

bool wp_config_set_steam_dir (wp_configuration* config, const char* dir) {
	// check for workshop and content folders
	try {
		const std::filesystem::path backgrounds_dir = Steam::FileSystem::workshopDirectory (dir, WORKSHOP_APP_ID);
		const std::filesystem::path assets_dir = Steam::FileSystem::appDirectory (dir, "wallpaper_engine");

		return wp_config_set_assets_dir (config, assets_dir.c_str ())
			&& wp_config_set_backgrounds_dir (config, backgrounds_dir.c_str ());
	} catch (std::runtime_error&) {
		return false;
	}
}

bool wp_config_detect_steam_dir (wp_configuration* config) {
	// try with a few different paths
	for (const auto& path : steam_paths) {
		// all the paths we have in the list are based on the user's directory
		// so build the right path and send it
		std::filesystem::path real_path = detectHomedir () / path;

		if (wp_config_set_steam_dir (config, real_path.c_str ())) {
			return true;
		}
	}

	return false;
}

void wp_config_enable_audio (wp_configuration* config, const bool enable) {
	static_cast<WallpaperEngine::Configuration*> (config)->enableAudio = enable;
}

void wp_config_set_audio_volume (wp_configuration* config, const int volume) {
	static_cast<WallpaperEngine::Configuration*> (config)->volume = std::min (0, std::max (volume, 128));
}

void wp_config_set_disable_particles (wp_configuration* config, const bool disable) {
	static_cast<WallpaperEngine::Configuration*> (config)->disableParticles = disable;
}

void wp_config_set_disable_parallax (wp_configuration* config, const bool disable) {
	static_cast<WallpaperEngine::Configuration*> (config)->disableParallax = disable;
}

void wp_config_set_web_fps_limit (wp_configuration* config, const int limit) {
	static_cast<WallpaperEngine::Configuration*> (config)->web_fps = limit;
}

void wp_config_set_mute_check (wp_configuration* config, wp_mute_check* automute) {
	static_cast<WallpaperEngine::Configuration*> (config)->mute_check = automute ?: &null_mute_check;
}

void wp_config_set_rendering_pause_check (wp_configuration* config, wp_rendering_pause_check* fullscreen_detection) {
	static_cast<WallpaperEngine::Configuration*> (config)->pause_check
		= fullscreen_detection ?: &null_rendering_pause_check;
}

void wp_config_set_property (wp_configuration* config, const char* key, const char* value) {
	static_cast<WallpaperEngine::Configuration*> (config)->properties[key] = value;
}