#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <optional>
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
    ApplicationContext (int argc, char* argv []);

    enum WINDOW_MODE {
        /** Default window mode */
        NORMAL_WINDOW = 0,
        /** Draw to the window server desktop */
        DESKTOP_BACKGROUND = 1,
        /** Explicit window mode with specified geometry */
        EXPLICIT_WINDOW = 2,
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

    struct {
        /**
         * General settings
         */
        struct {
            /** If the user requested a list of properties for the given background */
            bool onlyListProperties;
            /** If the user requested a dump of the background structure */
            bool dumpStructure;
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

            struct {
                /** The window size used in explicit window */
                glm::ivec4 geometry;
                TextureFlags clamp;
                WallpaperEngine::Render::WallpaperState::TextureUVsScaling scalingMode;
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
            .assets = "",
            .defaultBackground = "",
            .screenBackgrounds = {},
            .properties = {},
            .screenScalings = {},
            .screenClamps = {},
            .screenPlaylists = {},
            .defaultPlaylist = std::nullopt,
        },
        .render = {
            .mode = NORMAL_WINDOW,
            .maximumFPS = 30,
            .pauseOnFullscreen = true,
            .window = {
                .geometry = {},
                .clamp = TextureFlags_ClampUVs,
                .scalingMode = WallpaperEngine::Render::WallpaperState::TextureUVsScaling::DefaultUVs,
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

    [[nodiscard]] int getArgc() const;
    [[nodiscard]] char** getArgv() const;

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
    std::optional<PlaylistDefinition> buildPlaylistDefinition (const WallpaperEngine::Data::JSON::JSON& playlistJson,
                                                               const std::string& fallbackName) const;
    void registerPlaylist (PlaylistDefinition&& definition);
    [[nodiscard]] const PlaylistDefinition& getPlaylistFromConfig (const std::string& name);

    std::map<std::string, PlaylistDefinition> m_configPlaylists;
    bool m_loadedConfigPlaylists = false;
};
} // namespace WallpaperEngine::Application
