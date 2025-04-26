#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <glm/vec4.hpp>

#include "CApplicationState.h"

#include "WallpaperEngine/Assets/ITexture.h"
#include "WallpaperEngine/Render/CWallpaperState.h"

namespace WallpaperEngine::Application {
/**
 * Application information as parsed off the command line arguments
 */
class CApplicationContext {
  public:
    CApplicationContext (int argc, char* argv []);

    enum WINDOW_MODE {
        /** Default window mode */
        NORMAL_WINDOW = 0,
        /** Draw to the window server desktop */
        DESKTOP_BACKGROUND = 1,
        /** Explicit window mode with specified geometry */
        EXPLICIT_WINDOW = 2,
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
            std::map<std::string, WallpaperEngine::Render::CWallpaperState::TextureUVsScaling> screenScalings;
            /** The clamping mode for different screens */
            std::map<std::string, WallpaperEngine::Assets::ITexture::TextureFlags> screenClamps;
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
                WallpaperEngine::Assets::ITexture::TextureFlags clamp;
                WallpaperEngine::Render::CWallpaperState::TextureUVsScaling scalingMode;
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
        } mouse;

        /**
         * Screenshot settings
         */
        struct {
            /** If an screenshot should be taken */
            bool take;
            /** The frames to wait until the screenshot is taken */
            int delay;
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
        },
        .render = {
            .mode = NORMAL_WINDOW,
            .maximumFPS = 30,
            .pauseOnFullscreen = true,
            .window =
                {
                    .geometry = {},
                    .clamp = WallpaperEngine::Assets::ITexture::TextureFlags::ClampUVs,
                    .scalingMode = WallpaperEngine::Render::CWallpaperState::TextureUVsScaling::DefaultUVs,
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
        },
        .screenshot = {
            .take = false,
            .delay = 5,
            .path = "",
        },
    };

    CApplicationState state;

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
};
} // namespace WallpaperEngine::Application