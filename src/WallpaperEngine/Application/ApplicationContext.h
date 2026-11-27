#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec4.hpp>

#include "ApplicationState.h"
#include "WallpaperEngine/Data/JSON.h"

#include "../Render/TextureProvider.h"
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

    enum class PostProcessing { Disabled, Enabled, Ultra };
    /** Source mip selection and local image-effect resolution, independent of output render scale. */
    enum class TextureResolution { Auto = 0, Full = 1, Half = 2 };
    /** Native source policy at scene construction; output pixels are independent of render scale. */
    [[nodiscard]] static uint32_t calculateTextureReduction (
	TextureResolution quality, bool perspective, glm::ivec2 authoredSize, glm::ivec2 outputSize
    );
    /** How a live switch shares frames with the outgoing wallpaper while it loads */
    enum class SwitchPacing { Adaptive, Fixed, Off };

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
	    /** Properties to change values for */
	    std::map<std::string, std::string> properties;
	    /** The scaling mode for different screens */
	    std::map<std::string, WallpaperEngine::Render::WallpaperState::TextureUVsScaling> screenScalings;
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
	    /** Show measured rendering FPS on each output */
	    bool showFps;
	    /** Break the load-time HUD down into the phases that contributed */
	    bool showLoadDetails;
	    /** Scene render-target scale (1.0 = native, >1.0 = supersampling) */
	    float renderScale;
	    TextureResolution textureResolution;
	    /** Scene geometry multisample count (1 = off) */
	    uint32_t msaaSamples;
	    PostProcessing postProcessing;
	    bool planarReflections;
	    SwitchPacing switchPacing;
	    /** Final-output color grading (1.0 = neutral) */
	    float contrast;
	    float saturation;
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
	    /** Case-insensitive substrings of application names whose audio streams never
	     *  count as "something playing" for automute: wallpaper daemons keep phantom
	     *  streams open (skwd-paper holds an always-unmuted ALSA stream; stale
	     *  wallpaperengine instances leave their own), which would mute us forever. */
	    std::vector<std::string> automuteIgnore;
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
            .assets = "",
            .defaultBackground = "",
            .screenBackgrounds = {},
            .properties = {},
            .screenScalings = {},
            .screenClamps = {},
            .screenPlaylists = {},
            .defaultPlaylist = std::nullopt,
            .spanGroups = {},
        },
        .render = {
            .mode = NORMAL_WINDOW,
            .maximumFPS = 30,
            .showFps = false,
	    .showLoadDetails = false,
	    .renderScale = 1.0f,
	    .textureResolution = TextureResolution::Full,
	    .msaaSamples = 2,
	    .postProcessing = PostProcessing::Enabled,
	    .planarReflections = true,
	    .switchPacing = SwitchPacing::Adaptive,
	    .contrast = 1.0f,
	    .saturation = 1.0f,
            .pauseOnFullscreen = true,
            .pauseOnFullscreenOnlyWhenActive = false,
            .fullscreenPauseIgnoreAppIds = {},
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
            },
            .wayland = {
                .layer = WAYLAND_LAYER_BOTTOM,
            },
        },
        .audio = {
            .enabled = true,
            .volume = 15,
            .automute = true,
            .automuteIgnore = { "wallpaperengine", "skwd" },
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
    /** Native language code shared by the render and background-loading threads. */
    [[nodiscard]] std::string getLanguage () const;
    void setLanguage (const std::string& language);
    [[nodiscard]] static std::string normalizeLanguage (std::string language);

private:
    /** Program argument count on startup */
    int m_argc;
    /** Program arguments on startup */
    char** m_argv;
    mutable std::mutex m_languageMutex;
    std::string m_language = "en-us";

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

    std::map<std::string, PlaylistDefinition> m_configPlaylists;
    bool m_loadedConfigPlaylists = false;
};
} // namespace WallpaperEngine::Application
