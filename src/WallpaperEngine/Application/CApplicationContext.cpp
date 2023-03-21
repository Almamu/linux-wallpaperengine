#include "CApplicationContext.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Logging/CLog.h"

#include <cstring>
#include <string>
#include <getopt.h>

#define WORKSHOP_APP_ID 431960
#define APP_DIRECTORY "wallpaper_engine"

using namespace WallpaperEngine::Application;

struct option long_options [] = {
    {"screen-root",     required_argument, nullptr, 'r'},
	{"bg",				required_argument, nullptr, 'b'},
	{"window",			required_argument, nullptr, 'w'},
    {"pkg",             required_argument, nullptr, 'p'},
    {"dir",             required_argument, nullptr, 'd'},
    {"silent",          no_argument,       nullptr, 's'},
    {"volume",          required_argument, nullptr, 'v'},
    {"help",            no_argument,       nullptr, 'h'},
    {"fps",             required_argument, nullptr, 'f'},
    {"assets-dir",      required_argument, nullptr, 'a'},
    {"screenshot",      required_argument, nullptr, 'c'},
    {"list-properties", no_argument,       nullptr, 'l'},
    {"set-property",    required_argument, nullptr, 'o'},
    {nullptr,                           0, nullptr,   0}
};

std::string stringPathFixes(const std::string& s)
{
    if (s.empty ())
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

	std::string lastScreen;

    while ((c = getopt_long (argc, argv, "b:r:p:d:shf:a:w:", long_options, nullptr)) != -1)
    {
        switch (c)
        {
			case 'b':
				if (lastScreen.empty ())
					sLog.exception ("--bg has to go after a --screen-root argument");

				// no need to check for previous screen being in the list, as it's the only way for this variable
				// to have any value
				this->screenSettings.insert_or_assign (lastScreen, this->validatePath (optarg));
				break;

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
				if (this->screenSettings.find (optarg) != this->screenSettings.end ())
					sLog.exception ("Cannot specify the same screen more than once: ", optarg);
				if (this->windowMode == EXPLICIT_WINDOW)
					sLog.exception ("Cannot run in both background and window mode");

				this->windowMode = X11_BACKGROUND;
				lastScreen = optarg;
                this->screenSettings.insert_or_assign (lastScreen, "");
                break;

			case 'w':
				if (this->windowMode == X11_BACKGROUND)
					sLog.exception ("Cannot run in both background and window mode");

				if (optarg != nullptr)
				{
					this->windowMode = EXPLICIT_WINDOW;
					// read window geometry
					char* pos = optarg;

					if (pos != nullptr)
						this->windowGeometry.x = atoi (pos);
					if ((pos = strchr (pos, '.')) != nullptr)
						this->windowGeometry.y = atoi (pos + 1);
					if ((pos = strchr (pos + 1, '.')) != nullptr)
						this->windowGeometry.z = atoi (pos + 1);
					if ((pos = strchr (pos + 1, '.')) != nullptr)
						this->windowGeometry.w = atoi (pos + 1);
				}
				break;

            case 'p':
            case 'd':
                sLog.error ("--dir/--pkg is deprecated and not used anymore");
                this->background = this->validatePath (stringPathFixes (optarg));
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
                this->audioVolume = std::max (atoi (optarg), 128);
                break;

            case 'c':
                this->takeScreenshot = true;
                this->screenshot = stringPathFixes (optarg);
                break;

			default:
				sLog.out ("Default on path parsing: ", optarg);
				break;
        }
    }

    if (this->background.empty ())
    {
        if (optind < argc && strlen (argv [optind]) > 0)
        {
            this->background = this->validatePath (argv [optind]);
        }
        else
        {
            printHelp (argv [0]);
        }
    }

    // perform some extra validation on the inputs
    this->validateAssets ();
    this->validateScreenshot ();
}

std::string CApplicationContext::validatePath (const std::string& path)
{
	if (path.find ('/') == std::string::npos)
		return Steam::FileSystem::workshopDirectory (WORKSHOP_APP_ID, path);

	return path;
}

void CApplicationContext::validateAssets ()
{
    if (!this->assets.empty ())
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
    if (!this->takeScreenshot)
        return;

    if (!this->screenshot.has_extension ())
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
    sLog.out ("\t--volume <amount>\t\t\tSets the volume for all the sounds in the background");
    sLog.out ("\t--screen-root <screen name>\tDisplay as screen's background");
	sLog.out ("\t--window <geometry>\tRuns in window mode, geometry has to be XxYxWxH and sets the position and size of the window");
    sLog.out ("\t--fps <maximum-fps>\t\t\tLimits the FPS to the given number, useful to keep battery consumption low");
    sLog.out ("\t--assets-dir <path>\t\t\tFolder where the assets are stored");
    sLog.out ("\t--screenshot\t\t\t\tTakes a screenshot of the background");
    sLog.out ("\t--list-properties\t\t\tList all the available properties and their possible values");
    sLog.out ("\t--set-property <name=value>\tOverrides the default value of the given property");
}