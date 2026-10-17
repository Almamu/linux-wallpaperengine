#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "ApplicationState.h"
#include "WallpaperEngine/Data/JSON.h"

#include "../Render/TextureProvider.h"
#include "WallpaperEngine/Render/PostProcessSettings.h"
#include "WallpaperEngine/Render/WallpaperState.h"

#include "WallpaperEngine/Data/Model/Project.h"

namespace WallpaperEngine::Application {
using namespace WallpaperEngine::Data::Assets;
/**
 * Application information as parsed off the command line arguments
 */
class ApplicationContext {
public:
    ApplicationContext (int argc, char* argv[]);

    /**
     * Parses the given argc and argv and builds settings for the app
     */
    void loadSettingsFromArgv ();
    void printCatalogJson () const;

    enum WINDOW_MODE {
	/** Default window mode */
	NORMAL_WINDOW = 0,
	/** Draw to the window server desktop */
	DESKTOP_BACKGROUND = 1,
	/** Explicit window mode with specified geometry */
	EXPLICIT_WINDOW = 2,
    };

    /**
     * Wayland-only: which wlr-layer-shell layer to anchor the wallpaper surface to.
     * Different compositors treat layers differently; e.g. niri's
     * `place-within-backdrop` layer-rule only applies to BACKGROUND surfaces.
     */
    enum WAYLAND_LAYER {
	WAYLAND_LAYER_BACKGROUND = 0,
	WAYLAND_LAYER_BOTTOM = 1,
	WAYLAND_LAYER_TOP = 2,
	WAYLAND_LAYER_OVERLAY = 3,
    };

    struct PlaylistSettings {
	uint32_t delayMinutes = 60;
	std::string mode = "timer";
	std::string order = "sequential";
	bool updateOnPause = false;
	bool videoSequence = false;
    };

    struct PlaylistDefinition {
	std::string name;
	std::vector<std::filesystem::path> items;
	PlaylistSettings settings;
    };

    struct SpanGroup {
	std::vector<std::string> screens;
	std::filesystem::path background;
	WallpaperEngine::Render::WallpaperState::TextureUVsScaling scaling
	    = WallpaperEngine::Render::WallpaperState::TextureUVsScaling::DefaultUVs;
	TextureFlags clamp = TextureFlags_ClampUVs;
    };

    struct {
	/**
	 * General settings
	 */
	struct {
	    /** If the user requested a list of properties for the given background */
	    bool onlyListProperties;
	    /** If the user requested the installed Workshop catalog as machine-readable JSON */
	    bool catalogJson;
	    /** If the user requested a dump of the background structure */
	    bool dumpStructure;
	    /** If the user requested the particles to be deactivated */
	    bool disableParticles;
	    /** The path to the assets folder */
	    std::filesystem::path assets;
	    /** Background to load (provided as the final argument) as fallback for multi-screen setups */
	    std::filesystem::path defaultBackground;
	    /** The backgrounds specified for different screens */
	    std::map<std::string, std::filesystem::path> screenBackgrounds;
	    /** Properties to change values for globally */
	    std::map<std::string, std::string> properties;
	    /** Property overrides scoped to one output or span group */
	    std::map<std::string, std::map<std::string, std::string>> screenProperties;
	    /** The scaling mode for different screens */
	    std::map<std::string, WallpaperEngine::Render::WallpaperState::TextureUVsScaling> screenScalings;
	    /** UV offsets for individual outputs and span groups */
	    std::map<std::string, glm::vec2> screenOffsets;
	    /** Post-process controls for individual outputs and span groups */
	    std::map<std::string, WallpaperEngine::Render::PostProcessSettings> screenPostProcess;
	    /** The clamping mode for different screens */
	    std::map<std::string, TextureFlags> screenClamps;
	    /** Playlists selected per screen */
	    std::map<std::string, PlaylistDefinition> screenPlaylists;
	    /** Playlist used in window mode */
	    std::optional<PlaylistDefinition> defaultPlaylist;
	    /** Span groups: multiple monitors sharing one stretched wallpaper */
	    std::vector<SpanGroup> spanGroups;
	} general;

	/**
	 * Render settings
	 */
	struct {
	    /** The mode to run the background in */
	    WINDOW_MODE mode;
	    /** Maximum FPS */
	    int maximumFPS;
	    /** Indicates if pausing should happen when an app is maximized or fullscreen */
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
	    /** Post-process controls used when no output-specific override exists */
	    WallpaperEngine::Render::PostProcessSettings postProcess;
	    /** Render debugging switches for scene compatibility work */
	    struct {
		bool baseOnly;
		bool noSolidFinal;
		bool passLog;
		std::optional<int> objectFilter;
		std::vector<int> skipObjects;
		std::vector<int> skipEffects;
	    } debug;

	    struct {
		/** The window size used in explicit window */
		glm::ivec4 geometry;
		TextureFlags clamp;
		WallpaperEngine::Render::WallpaperState::TextureUVsScaling scalingMode;
		glm::vec2 uvOffset;
	    } window;

	    struct {
		/** Which wlr-layer-shell layer to use for desktop backgrounds */
		WAYLAND_LAYER layer;
	    } wayland;
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
            .catalogJson = false,
            .dumpStructure = false,
            .assets = "",
            .defaultBackground = "",
            .screenBackgrounds = {},
            .properties = {},
            .screenProperties = {},
            .screenScalings = {},
            .screenOffsets = {},
            .screenPostProcess = {},
            .screenClamps = {},
            .screenPlaylists = {},
            .defaultPlaylist = std::nullopt,
            .spanGroups = {},
        },
        .render = {
            .mode = NORMAL_WINDOW,
            .maximumFPS = 30,
            .pauseOnFullscreen = true,
            .pauseOnFullscreenOnlyWhenActive = false,
            .fullscreenPauseIgnoreAppIds = {},
            .postProcess = {},
            .debug = {
                .baseOnly = false,
                .noSolidFinal = false,
	                .passLog = false,
	                .objectFilter = std::nullopt,
	                .skipObjects = {},
	                .skipEffects = {},
	            },
            .window = {
                .geometry = {},
                .clamp = TextureFlags_ClampUVs,
                .scalingMode = WallpaperEngine::Render::WallpaperState::TextureUVsScaling::DefaultUVs,
                .uvOffset = { 0.0f, 0.0f },
            },
            .wayland = {
                .layer = WAYLAND_LAYER_BOTTOM,
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

    ApplicationState state;

    [[nodiscard]] int getArgc () const;
    [[nodiscard]] char** getArgv () const;

private:
    /** Program argument count on startup */
    int m_argc;
    /** Program arguments on startup */
    char** m_argv;

    /**
     * Validates the assets folder and ensures a valid one is present
     */
    void validateAssets ();

    /**
     * Validates the screenshot settings
     */
    void validateScreenshot () const;

    /**
     * Validates a background parameter and returns the real bgIdOrPath to it
     *
     * @param bgIdOrPath
     * @return
     */
    static std::filesystem::path translateBackground (const std::string& bgIdOrPath);

    void loadPlaylistsFromConfig ();
    std::filesystem::path resolvePlaylistItemPath (const std::string& raw) const;
    std::filesystem::path configFilePath () const;
    std::optional<WallpaperEngine::Data::JSON::JSON> parseConfigJson (const std::filesystem::path& path) const;
    PlaylistSettings parsePlaylistSettings (const WallpaperEngine::Data::JSON::JSON& playlistJson) const;
    std::vector<std::filesystem::path>
    collectPlaylistItems (const WallpaperEngine::Data::JSON::JSON& playlistJson, const std::string& name) const;
    std::optional<PlaylistDefinition> buildPlaylistDefinition (
	const WallpaperEngine::Data::JSON::JSON& playlistJson, const std::string& fallbackName
    ) const;
    void registerPlaylist (PlaylistDefinition&& definition);
    [[nodiscard]] const PlaylistDefinition& getPlaylistFromConfig (const std::string& name);

    void applyWindowGeometry (const std::string& value);
    void selectScreenRoot (const std::string& value, std::string& lastScreen);
    void selectScreenSpan (const std::string& value, std::string& lastScreen);
    void applyScreenBackground (const std::string& value, const std::string& lastScreen);
    void applyPlaylist (const std::string& value, const std::string& lastScreen);
    void applyScaling (const std::string& value, const std::string& lastScreen);
    void applyOffset (const std::string& value, const std::string& lastScreen, bool horizontal);
    void applyClamp (const std::string& value, const std::string& lastScreen);
    void applyWaylandLayer (const std::string& value);
    void applyPostProcessValue (const std::string& value, const std::string& lastScreen, bool contrast);
    void applyBorderColour (const std::string& value, const std::string& lastScreen);
    void applyScreenProperty (const std::string& value, const std::string& lastScreen);
    void applyRenderDebug (const std::string& value);

    std::map<std::string, PlaylistDefinition> m_configPlaylists;
    bool m_loadedConfigPlaylists = false;
};
} // namespace WallpaperEngine::Application
