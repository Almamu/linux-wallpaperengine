#pragma once

#include <FreeImage.h>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <glm/vec4.hpp>

namespace WallpaperEngine::Application
{
    class CApplicationContext
    {
    public:
        CApplicationContext (int argc, char* argv[]);

		enum WINDOW_MODE
		{
			NORMAL_WINDOW = 0,
			X11_BACKGROUND = 1,
			EXPLICIT_WINDOW = 2,
		};

		glm::ivec4 windowGeometry;
        std::map <std::string, std::string> screenSettings;
        std::map <std::string, std::string> properties;
        std::string background;
        std::filesystem::path assets;
        std::filesystem::path screenshot;
        bool takeScreenshot;
        int maximumFPS;
        int audioVolume;
        bool audioEnabled;
        bool onlyListProperties;
        FREE_IMAGE_FORMAT screenshotFormat;
		WINDOW_MODE windowMode;

    private:
		std::string validatePath (const std::string& path);
        void validateAssets ();
        void validateScreenshot ();
        static void printHelp (const char* route);
    };
}