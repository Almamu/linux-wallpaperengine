#include "CApplicationContext.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Logging/CLog.h"

#include <cstring>
#include <getopt.h>
#include <glm/common.hpp>
#include <string>

#define WORKSHOP_APP_ID 431960
#define APP_DIRECTORY "wallpaper_engine"

using namespace WallpaperEngine::Application;

struct option long_options [] = {
    {"screen-root",     required_argument, 0, 'r'},
    {"pkg",             required_argument, 0, 'p'},
    {"dir",             required_argument, 0, 'd'},
    {"silent",          no_argument,       0, 's'},
    {"volume",          required_argument, 0, 'v'},
    {"help",            no_argument,       0, 'h'},
    {"fps",             required_argument, 0, 'f'},
    {"assets-dir",      required_argument, 0, 'a'},
    {"screenshot",      required_argument, 0, 'c'},
    {"list-properties", no_argument,       0, 'l'},
    {"set-property",    required_argument, 0, 'o'},
    {nullptr,                           0, 0,   0}
};

std::string stringPathFixes(const std::string& s)
{
    if (s.empty () == true)
        return s;

    std::string str (s);

    // remove single-quotes from the arguments
    if (str [0] == '\'' && str [str.size() - 1] == '\'')
        str
            .erase (str.size() - 1, 1)
            .erase (0, 1);

    return std::move (str);
}

CApplicationContext::CApplicationContext (int argc, char* argv[]) :
    takeScreenshot (false),
    maximumFPS (30),
    audioVolume (128),
    audioEnabled (true),
    onlyListProperties (false)
{
    int c;

    while ((c = getopt_long (argc, argv, "r:p:d:shf:a:", long_options, nullptr)) != -1)
    {
        switch (c)
        {
            case 'o':
                {
                    std::string value = optarg;
                    std::string::size_type equals = value.find ('=');

                    // properties without value are treated as booleans for now
                    if (equals == std::string::npos)
                        this->properties.insert_or_assign (value, "1");
                    else
                        this->properties.insert_or_assign (
                            value.substr (0, equals),
                            value.substr (equals + 1)
                        );
                }
                break;

            case 'l':
                this->onlyListProperties = true;
                break;

            case 'r':
                screens.emplace_back (optarg);
                break;

            case 'p':
            case 'd':
                sLog.error ("--dir/--pkg is deprecated and not used anymore");
                this->background = stringPathFixes (optarg);
                break;

            case 's':
                this->audioEnabled = false;
                break;

            case 'h':
                this->printHelp (argv [0]);
                break;

            case 'f':
                maximumFPS = atoi (optarg);
                break;

            case 'a':
                this->assets = stringPathFixes (optarg);
                break;

            case 'v':
                this->audioVolume = glm::clamp (atoi (optarg), 0, 128);
                break;

            case 'c':
                this->takeScreenshot = true;
                this->screenshot = stringPathFixes (optarg);
                break;
        }
    }

    if (this->background.empty () == true)
    {
        if (optind < argc && strlen (argv [optind]) > 0)
        {
            this->background = argv [optind];
        }
        else
        {
            this->printHelp (argv [0]);
        }
    }

    // perform some extra validation on the inputs
    this->validatePath ();
    this->validateAssets ();
    this->validateScreenshot ();
}

void CApplicationContext::validatePath ()
{
    if (this->background.find ('/') != std::string::npos)
        return;

    this->background = Steam::FileSystem::workshopDirectory (WORKSHOP_APP_ID, this->background);
}

void CApplicationContext::validateAssets ()
{
    if (this->assets.empty () == false)
    {
        sLog.out ("Using wallpaper engine's assets at ", this->assets, " based on --assets-dir parameter");
        return;
    }

    try
    {
        this->assets = Steam::FileSystem::appDirectory (APP_DIRECTORY, "assets");
    }
    catch (std::runtime_error&)
    {
        // set current path as assets' folder
        std::filesystem::path directory = std::filesystem::canonical ("/proc/self/exe")
                                              .parent_path () / "assets";
    }
}

void CApplicationContext::validateScreenshot ()
{
    if (this->takeScreenshot == false)
        return;

    if (this->screenshot.has_extension () == false)
        sLog.exception ("Cannot determine screenshot format");

    std::string extension = this->screenshot.extension ();

    if (extension == ".bmp")
        this->screenshotFormat = FIF_BMP;
    else if (extension == ".png")
        this->screenshotFormat = FIF_PNG;
    else if (extension == ".jpg" || extension == ".jpeg")
        this->screenshotFormat = FIF_JPEG;
    else
        sLog.exception ("Cannot determine screenshot format, unknown extension ", extension);
}

void CApplicationContext::printHelp (const char* route)
{
    sLog.out ("Usage: ", route, " [options] background_path/background_id");
    sLog.out ("");
    sLog.out ("where background_path/background_id can be:");
    sLog.out ("\tthe ID of the background (for autodetection on your steam installation)");
    sLog.out ("\ta full path to the background's folder");
    sLog.out ("");
    sLog.out ("options:");
    sLog.out ("\t--silent\t\t\t\t\tMutes all the sound the wallpaper might produce");
    sLog.out ("\t--volume <amount>\t\t\tSets the volume for all the sounds in the background (0 - 128)");
    sLog.out ("\t--screen-root <screen name>\tDisplay as screen's background");
    sLog.out ("\t--fps <maximum-fps>\t\t\tLimits the FPS to the given number, useful to keep battery consumption low");
    sLog.out ("\t--assets-dir <path>\t\t\tFolder where the assets are stored");
    sLog.out ("\t--screenshot\t\t\t\tTakes a screenshot of the background");
    sLog.out ("\t--list-properties\t\t\tList all the available properties and their possible values");
    sLog.out ("\t--set-property <name=value>\tOverrides the default value of the given property");
}