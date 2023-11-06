#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <glm/vec4.hpp>

#include <FreeImage.h>

#include "CApplicationState.h"

#include "WallpaperEngine/Assets/ITexture.h"

namespace WallpaperEngine::Application
{
	/**
	 * Application information as parsed off the command line arguments
	 */
	class CApplicationContext
	{
	public:
		CApplicationContext (int argc, char* argv[]);

		enum WINDOW_MODE
		{
			/** Default window mode */
			NORMAL_WINDOW = 0,
			/** Draw to the window server desktop */
			DESKTOP_BACKGROUND = 1,
			/** Explicit window mode with specified geometry */
			EXPLICIT_WINDOW = 2,
		};

        struct
        {
            /**
             * General settings
             */
            struct
            {
                /** If the user requested a list of properties for the given background */
                bool onlyListProperties;

                /** The path to the assets folder */
                std::filesystem::path assets;
                /** Background to load (provided as the final argument) as fallback for multi-screen setups */
                std::filesystem::path defaultBackground;

                /** The backgrounds specified for different screens */
                std::map <std::string, std::filesystem::path> screenBackgrounds;
                /** Properties to change values for */
                std::map <std::string, std::string> properties;
            } general;

            /**
             * Render settings
             */
            struct
            {
                /** The mode to run the background in */
                WINDOW_MODE mode;
                /** Maximum FPS */
                int maximumFPS;
                /** Indicates if pausing should happen when something goes fullscreen */
                bool pauseOnFullscreen;

                struct
                {
                    /** The window size used in explicit window */
                    glm::ivec4 geometry;
                    WallpaperEngine::Assets::ITexture::TextureFlags clamp;
                    bool scaleToFit;
                } window;
            } render;

            /**
             * Audio settings
             */
            struct
            {
                /** If the audio system is enabled */
                bool enabled;
                /** Sound volume (0-128) */
                int volume;
                /** If the audio must be muted if something else is playing sound */
                bool automute;
            } audio;

            /**
             * Mouse input settings
             */
            struct
            {
                /** If the mouse movement is enabled */
                bool enabled;
            } mouse;

            /**
             * Screenshot settings
             */
            struct
            {
                /** If an screenshot should be taken */
                bool take;
                /** The path to where the screenshot must be saved */
                std::filesystem::path path;
                /** The image format */
                FREE_IMAGE_FORMAT format;
            } screenshot;
        } settings;

        CApplicationState state;

	private:
		/**
		 * Validates the assets folder and ensures a valid one is present
		 */
		void validateAssets ();

		/**
		 * Validates the screenshot settings
		 */
		void validateScreenshot ();

		/**
		 * Validates a background parameter and returns the real bgIdOrPath to it
		 *
		 * @param bgIdOrPath
		 * @return
		 */
		static std::filesystem::path translateBackground (const std::string& bgIdOrPath);

		/**
		 * Prints the normal help message
		 */
		static void printHelp (const char* route);
	};
}