#pragma once

#include <FreeImage.h>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace WallpaperEngine::Application
{
    class CApplicationContext
    {
    public:
        CApplicationContext (int argc, char* argv[]);

        std::vector <std::string> screens;
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
        std::string window_class;

    private:
        void validatePath ();
        void validateAssets ();
        void validateScreenshot ();
        static void printHelp (const char* route);
    };
}