#ifndef __WP_LIB_CONFIGURATION_H__
#define __WP_LIB_CONFIGURATION_H__

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WORKSHOP_APP_ID 431960

/**
 * Configuration instance
 */
typedef void wp_configuration;

/**
 * Pointers to different functions to check if the audio should be muted
 * (only called when enabled)
 */
struct wp_mute_check {
	/**
	 * Pointer to user-defined data that will be passed to the callbacks
	 */
	void* user_parameter;

	bool (*is_muted) (void* user_parameter);
};

/**
 * Pointers to different functions to check if rendering should be paused
 */
struct wp_rendering_pause_check {
	/**
	 * Pointer to user-defined data that will be passed to the callbacks
	 */
	void* user_parameter;
	/**
	 * Callback that should return if something is fullscreen or not, used for pausing
	 */
	bool (*is_paused) (void* user_parameter);
};

/**
 * Allocates and sets up some sensible defaults for the renderer
 *
 * @return
 */
WPENGINE_API wp_configuration* wp_config_create ();

/**
 * Frees any allocated memory by the configuration instance
 * and itself
 *
 * @param config The config to free
 */
WPENGINE_API void wp_config_destroy (wp_configuration* config);

/**
 * Updates the assets directory for this configuration
 *
 * @param config
 * @param dir
 *
 * @return true if the directory was set successfully, false otherwise
 */
WPENGINE_API bool wp_config_set_assets_dir (wp_configuration* config, const char* dir);

/**
 * Updates the directory where backgrounds are stored for this configuration
 *
 * @param config
 * @param dir
 *
 * @return true if the directory was set successfully, false otherwise
 */
WPENGINE_API bool wp_config_set_backgrounds_dir (wp_configuration* config, const char* dir);

/**
 * Updates the steam directory for this configuration.
 *
 * This calls wp_config_set_assets_dir with the default directory for you
 *
 * @param config
 * @param dir
 *
 * @return true if the directory was set successfully, false otherwise
 */
WPENGINE_API bool wp_config_set_steam_dir (wp_configuration* config, const char* dir);

/**
 * Checks common folders in search of the steam directory.
 * If a valid folder is found, this also calls wp_config_set_steam_dir for you
 *
 * @param config
 *
 * @return true if the steam directory was detected, false otherwise
 */
WPENGINE_API bool wp_config_detect_steam_dir (wp_configuration* config);

/**
 * Enables/disables the audio playing
 *
 * @param config
 * @param enable
 */
WPENGINE_API void wp_config_enable_audio (wp_configuration* config, bool enable);

/**
 * Sets the desired background's volume (value ranges from 0 to 128)
 *
 * @param config
 * @param volume
 */
WPENGINE_API void wp_config_set_audio_volume (wp_configuration* config, int volume);

/**
 * Enables/disables particle rendering for backgrounds under this configuration
 *
 * @param config
 * @param disable
 */
WPENGINE_API void wp_config_set_disable_particles (wp_configuration* config, bool disable);

/**
 * Enables/disables parallax effects on backgrounds
 *
 * @param config
 * @param disable
 */
WPENGINE_API void wp_config_set_disable_parallax (wp_configuration* config, bool disable);

/**
 * Sets the FPS limit for web-based rendering
 *
 * @param config
 * @param limit
 */
WPENGINE_API void wp_config_set_web_fps_limit (wp_configuration* config, int limit);

/**
 * Enables automute and configurates detection
 *
 * @param config The configuration instance to modify
 * @param automute The automute configuration to apply, null to disabled automute
 */
WPENGINE_API void wp_config_set_mute_check (wp_configuration* config, wp_mute_check* automute);

/**
 * Enables fullscreen detection and configurates it
 *
 * @param config The configuration instance to modify
 * @param fullscreen_detection The fullscreen detection configuration to apply null to disable fullscreen detection
 */
WPENGINE_API void
wp_config_set_rendering_pause_check (wp_configuration* config, wp_rendering_pause_check* fullscreen_detection);

/**
 * Sets a value for a background's property to override the defaults
 *
 * @param config
 * @param key The property to set the value for
 * @param value Property's value
 */
WPENGINE_API void wp_config_set_property (wp_configuration* config, const char* key, const char* value);

#ifdef __cplusplus
}
#endif

#endif