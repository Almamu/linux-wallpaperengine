#include <algorithm>
#include <vector>

#include "frontends/configuration.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Configuration.h"

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

#define WPENGINE_CONFIG_API_BEGIN try {
#define WPENGINE_CONFIG_API_END(result)                                                                                \
	}                                                                                                                  \
	catch (...) {                                                                                                      \
		return result;                                                                                                 \
	}

WPENGINE_API wp_configuration* wp_config_create () {
	WPENGINE_CONFIG_API_BEGIN
	return new WallpaperEngine::Configuration (&null_rendering_pause_check, &null_mute_check);
	WPENGINE_CONFIG_API_END (nullptr);
}

WPENGINE_API void wp_config_destroy (wp_configuration* config) {
	WPENGINE_CONFIG_API_BEGIN
	delete static_cast<WallpaperEngine::Configuration*> (config);
	WPENGINE_CONFIG_API_END ();
}

WPENGINE_API bool wp_config_set_assets_dir (wp_configuration* config, const char* dir) {
	WPENGINE_CONFIG_API_BEGIN
	return static_cast<WallpaperEngine::Configuration*> (config)->setAssetsDir (dir);
	WPENGINE_CONFIG_API_END (false);
}

WPENGINE_API bool wp_config_set_backgrounds_dir (wp_configuration* config, const char* dir) {
	WPENGINE_CONFIG_API_BEGIN
	return static_cast<WallpaperEngine::Configuration*> (config)->setBackgroundsDir (dir);
	WPENGINE_CONFIG_API_END (false);
}

WPENGINE_API bool wp_config_set_steam_dir (wp_configuration* config, const char* dir) {
	WPENGINE_CONFIG_API_BEGIN
	return static_cast<WallpaperEngine::Configuration*> (config)->setSteamDir (dir);
	WPENGINE_CONFIG_API_END (false);
}

WPENGINE_API bool wp_config_detect_steam_dir (wp_configuration* config) {
	WPENGINE_CONFIG_API_BEGIN
	return static_cast<WallpaperEngine::Configuration*> (config)->detectSteamDir ();
	WPENGINE_CONFIG_API_END (false);
}

WPENGINE_API void wp_config_enable_audio (wp_configuration* config, const bool enable) {
	WPENGINE_CONFIG_API_BEGIN
	static_cast<WallpaperEngine::Configuration*> (config)->enableAudio = enable;
	WPENGINE_CONFIG_API_END ();
}

WPENGINE_API void wp_config_set_audio_volume (wp_configuration* config, const int volume) {
	WPENGINE_CONFIG_API_BEGIN
	static_cast<WallpaperEngine::Configuration*> (config)->volume = std::min (0, std::max (volume, 128));
	WPENGINE_CONFIG_API_END ();
}

WPENGINE_API void wp_config_set_disable_particles (wp_configuration* config, const bool disable) {
	WPENGINE_CONFIG_API_BEGIN
	static_cast<WallpaperEngine::Configuration*> (config)->disableParticles = disable;
	WPENGINE_CONFIG_API_END ();
}

WPENGINE_API void wp_config_set_disable_parallax (wp_configuration* config, const bool disable) {
	WPENGINE_CONFIG_API_BEGIN
	static_cast<WallpaperEngine::Configuration*> (config)->disableParallax = disable;
	WPENGINE_CONFIG_API_END ();
}

WPENGINE_API void wp_config_set_web_fps_limit (wp_configuration* config, const int limit) {
	WPENGINE_CONFIG_API_BEGIN
	static_cast<WallpaperEngine::Configuration*> (config)->web_fps = limit;
	WPENGINE_CONFIG_API_END ();
}

WPENGINE_API void wp_config_set_mute_check (wp_configuration* config, wp_mute_check* automute) {
	WPENGINE_CONFIG_API_BEGIN
	static_cast<WallpaperEngine::Configuration*> (config)->mute_check = automute ?: &null_mute_check;
	WPENGINE_CONFIG_API_END ();
}

WPENGINE_API void
wp_config_set_rendering_pause_check (wp_configuration* config, wp_rendering_pause_check* fullscreen_detection) {
	WPENGINE_CONFIG_API_BEGIN
	static_cast<WallpaperEngine::Configuration*> (config)->pause_check
		= fullscreen_detection ?: &null_rendering_pause_check;
	WPENGINE_CONFIG_API_END ();
}

WPENGINE_API void wp_config_set_property (wp_configuration* config, const char* key, const char* value) {
	WPENGINE_CONFIG_API_BEGIN
	static_cast<WallpaperEngine::Configuration*> (config)->properties[key] = value;
	WPENGINE_CONFIG_API_END ();
}