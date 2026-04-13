#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <linux-wallpaperengine/configuration.h>
#include <linux-wallpaperengine/context.h>
#include <linux-wallpaperengine/playlists.h>

#include <glm/vec4.hpp>

#define DEFAULT_SCREEN_NAME "__default__screen__"

namespace WallpaperEngine::Application {
/**
 * Application information as parsed off the command line arguments
 */
class ApplicationContext {
public:
	ApplicationContext (int argc, char* argv[], wp_configuration* config);

	/**
	 * Parses the given argc and argv and builds settings for the app
	 */
	void loadSettingsFromArgv ();

	enum WINDOW_MODE {
		/** Default window mode */
		NORMAL_WINDOW = 0,
		/** Draw to the window server desktop */
		DESKTOP_BACKGROUND = 1,
		/** Explicit window mode with specified geometry */
		EXPLICIT_WINDOW = 2,
	};

	enum SCALING_MODE {
		SCALING_MODE_STRETCH = 0,
		SCALING_MODE_FIT = 1,
		SCALING_MODE_FILL = 2,
		SCALING_MODE_DEFAULT = 3,
	};

	enum CLAMP_MODE { CLAMP_MODE_UVS = 0, CLAMP_MODE_UVS_BORDER = 1, CLAMP_MODE_REPEAT = 2 };

	struct PlaylistSettings {
		uint32_t delayMinutes;
		wp_playlist_mode mode;
		wp_playlist_order order;
		bool updateOnPause;
		bool videoSequence;
	};

	struct PlaylistDefinition {
		std::string name;
		std::vector<std::string> items;
		PlaylistSettings settings;
	};

	struct {
		/**
		 * General settings
		 */
		struct {
			/** If the user requested a list of properties for the given background */
			bool onlyListProperties;
			/** If the user requested a dump of the background structure */
			bool dumpStructure;
			/** If the user requested the particles to be deactivated */
			bool disableParticles;
			/** The path to the assets folder */
			std::optional<std::filesystem::path> assets;
			/** The path to the steam folder */
			std::optional<std::filesystem::path> steam;
			/** The backgrounds specified for different screens */
			std::map<std::string, std::string> backgrounds;
			/** Properties to change values for */
			std::map<std::string, std::string> properties;
			/** The scaling mode for different screens */
			std::map<std::string, SCALING_MODE> screenScalings;
			/** The clamping mode for different screens */
			std::map<std::string, CLAMP_MODE> screenClamps;
			/** Playlists selected per screen */
			std::map<std::string, std::string> playlists;
		} general;

		/**
		 * Render settings
		 */
		struct {
			/** The mode to run the background in */
			WINDOW_MODE mode;
			/** Maximum FPS */
			int maximumFPS;
			/** Indicates if pausing should happen when something goes fullscreen */
			bool pauseOnFullscreen;
			/**
			 * Wayland-only: if true, only consider fullscreen toplevels that are also activated.
			 * Useful for compositors with "virtual" fullscreen windows (e.g. scrollable tiling).
			 */
			bool pauseOnFullscreenOnlyWhenActive;
			/**
			 * Wayland-only: list of app_id substrings to ignore for fullscreen pause.
			 * Example: "firefox" will match "org.mozilla.firefox".
			 */
			std::vector<std::string> fullscreenPauseIgnoreAppIds;

			struct {
				/** The window size used in explicit window */
				glm::ivec4 geometry;
				CLAMP_MODE clamp;
				SCALING_MODE scalingMode;
			} window;
		} render;

		/**
		 * Audio settings
		 */
		struct {
			/** If the audio system is enabled */
			bool enabled;
			/** Sound volume (0-128) */
			int volume;
			/** If the audio must be muted if something else is playing sound */
			bool automute;
			/** If audio processing can be enabled or not */
			bool audioprocessing;
		} audio;

		/**
		 * Mouse input settings
		 */
		struct {
			/** If the mouse movement is enabled */
			bool enabled;
			/** If the mouse parallax should be disabled */
			bool disableparallax;
		} mouse;

		/**
		 * Screenshot settings
		 */
		struct {
			/** If an screenshot should be taken */
			bool take;
			/** The frames to wait until the screenshot is taken */
			uint32_t delay;
			/** The path to where the screenshot must be saved */
			std::filesystem::path path;
		} screenshot;
	} settings = {
        .general = {
            .onlyListProperties = false,
            .dumpStructure = false,
            .assets = std::nullopt,
        	.steam = std::nullopt,
            .backgrounds = {},
            .properties = {},
            .screenScalings = {},
            .screenClamps = {},
            .playlists = {},
        },
        .render = {
            .mode = NORMAL_WINDOW,
            .maximumFPS = 30,
            .pauseOnFullscreen = true,
            .pauseOnFullscreenOnlyWhenActive = false,
            .fullscreenPauseIgnoreAppIds = {},
            .window = {
                .geometry = {},
                .clamp = CLAMP_MODE_UVS,
                .scalingMode = SCALING_MODE_DEFAULT,
            },
        },
        .audio = {
            .enabled = true,
            .volume = 15,
            .automute = true,
            .audioprocessing = true,
        },
        .mouse = {
            .enabled = true,
            .disableparallax = false,
        },
        .screenshot = {
            .take = false,
            .delay = 5,
            .path = "",
        },
    };

	struct {
		bool keepRunning;
		wp_context* context;
	} state = {
		.keepRunning = true,
		.context = nullptr,
	};

	[[nodiscard]] int getArgc () const;
	[[nodiscard]] char** getArgv () const;
	[[nodiscard]] wp_configuration* getConfig () const;
	[[nodiscard]] const std::map<std::string, PlaylistDefinition>& getPlaylists () const;

private:
	/** Program argument count on startup */
	int m_argc;
	/** Program arguments on startup */
	char** m_argv;
	/** wp library configuration */
	wp_configuration* m_config;
	/** playlists definitions for the playlists we hold a reference to */
	std::map<std::string, PlaylistDefinition> m_playlists;

	/**
	 * Validates the assets folder and ensures a valid one is present
	 */
	void validateAssets () const;

	/**
	 * Validates the playlists exist and are valid
	 */
	void validatePlaylists ();

	/**
	 * Validates the screenshot settings
	 */
	void validateScreenshot () const;

	std::map<std::string, PlaylistDefinition> m_configPlaylists;
	bool m_loadedConfigPlaylists = false;
};
} // namespace WallpaperEngine::Application
