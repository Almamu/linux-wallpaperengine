#include "CApplicationContext.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Logging/CLog.h"

#include <cstdlib>
#include <cstring>
#include <getopt.h>

#define WORKSHOP_APP_ID 431960
#define APP_DIRECTORY "wallpaper_engine"

using namespace WallpaperEngine::Application;

struct option long_options[] = {
    { "screen-root", required_argument,     nullptr, 'r' },
    { "bg", required_argument,              nullptr, 'b' },
    { "window", required_argument,          nullptr, 'w' },
    { "pkg", required_argument,             nullptr, 'p' },
    { "dir", required_argument,             nullptr, 'd' },
    { "silent", no_argument,                nullptr, 's' },
    { "volume", required_argument,          nullptr, 'v' },
    { "help", no_argument,                  nullptr, 'h' },
    { "fps", required_argument,             nullptr, 'f' },
    { "assets-dir", required_argument,      nullptr, 'a' },
    { "screenshot", required_argument,      nullptr, 'c' },
    { "list-properties", no_argument,       nullptr, 'l' },
    { "set-property", required_argument,    nullptr, 'o' },
    { "noautomute", no_argument,            nullptr, 'm' },
    { "no-fullscreen-pause", no_argument,   nullptr, 'n' },
    { "disable-mouse", no_argument,         nullptr, 'e' },
    { nullptr, 0,                           nullptr, 0 }
};

std::string stringPathFixes (const std::string& s)
{
    if (s.empty ())
        return s;

    std::string str (s);

    // remove single-quotes from the arguments
    if (str[0] == '\'' && str[str.size () - 1] == '\'')
        str
            .erase (str.size () - 1, 1)
            .erase (0, 1);

    return std::move (str);
}

CApplicationContext::CApplicationContext (int argc, char* argv[])
{
    // setup structs with sane default values for now
    this->settings =
    {
        .general =
        {
            .onlyListProperties = false,
            .assets = "",
            .defaultBackground = "",
            .screenBackgrounds = {},
            .properties = {},
        },
        .render =
        {
            .mode = NORMAL_WINDOW,
            .maximumFPS = 30,
            .pauseOnFullscreen = true,
            .window = { .geometry = {}},
        },
        .audio =
        {
            .enabled = true,
            .volume = 15,
            .automute = true
        },
        .mouse =
        {
            .enabled = true,
        },
        .screenshot =
        {
            .take = false,
            .path = "",
            .format = FIF_UNKNOWN,
        },
    };

    int c;

    std::string lastScreen;

    while ((c = getopt_long (argc, argv, "b:r:p:d:shf:a:w:mn", long_options, nullptr)) != -1)
    {
        switch (c)
        {
            case 'n':
                this->settings.render.pauseOnFullscreen = false;
                break;

            case 'b':
                if (lastScreen.empty ())
                    sLog.exception ("--bg has to go after a --screen-root argument");

                // no need to check for previous screen being in the list, as it's the only way for this variable
                // to have any value
                this->settings.general.screenBackgrounds[lastScreen] = translateBackground (optarg);
                break;

            case 'o':
                {
                    std::string value = optarg;
                    std::string::size_type equals = value.find ('=');

                    // properties without value are treated as booleans for now
                    if (equals == std::string::npos)
                        this->settings.general.properties[value] = "1";
                    else
                        this->settings.general.properties[value.substr (0, equals)] = value.substr (equals + 1);
                }
                break;

            case 'l':
                this->settings.general.onlyListProperties = true;
                break;

            case 'r':
                if (this->settings.general.screenBackgrounds.find (optarg) != this->settings.general.screenBackgrounds.end ())
                    sLog.exception ("Cannot specify the same screen more than once: ", optarg);
                if (this->settings.render.mode == EXPLICIT_WINDOW)
                    sLog.exception ("Cannot run in both background and window mode");

                this->settings.render.mode = DESKTOP_BACKGROUND;
                lastScreen = optarg;
                this->settings.general.screenBackgrounds[lastScreen] = "";
                break;

            case 'w':
                if (this->settings.render.mode == DESKTOP_BACKGROUND)
                    sLog.exception ("Cannot run in both background and window mode");

                if (optarg != nullptr)
                {
                    this->settings.render.mode = EXPLICIT_WINDOW;
                    // read window geometry
                    char* pos = optarg;

                    if (pos != nullptr)
                        this->settings.render.window.geometry.x = atoi (pos);
                    if ((pos = strchr (pos, 'x')) != nullptr)
                        this->settings.render.window.geometry.y = atoi (pos + 1);
                    if ((pos = strchr (pos + 1, 'x')) != nullptr)
                        this->settings.render.window.geometry.z = atoi (pos + 1);
                    if ((pos = strchr (pos + 1, 'x')) != nullptr)
                        this->settings.render.window.geometry.w = atoi (pos + 1);
                }
                break;

            case 'p':
            case 'd':
                sLog.error ("--dir/--pkg is deprecated and not used anymore");
                this->settings.general.defaultBackground = translateBackground (stringPathFixes (optarg));
                break;

            case 's':
                this->settings.audio.enabled = false;
                break;

            case 'h':
                printHelp (argv[0]);
		std::exit(0);
                break;

            case 'f':
                this->settings.render.maximumFPS = atoi (optarg);
                break;

            case 'a':
                this->settings.general.assets = stringPathFixes (optarg);
                break;

            case 'v':
                this->settings.audio.volume = std::max (atoi (optarg), 128);
                break;

            case 'c':
                this->settings.screenshot.take = true;
                this->settings.screenshot.path = stringPathFixes (optarg);
                break;

            case 'm':
                this->settings.audio.automute = false;
                break;

            case 'e':
                this->settings.mouse.enabled = false;
                break;

            default:
                sLog.out ("Default on path parsing: ", optarg);
                break;
        }
    }

    if (this->settings.general.defaultBackground.empty ())
    {
        if (optind < argc && strlen (argv[optind]) > 0)
        {
            this->settings.general.defaultBackground = translateBackground (argv[optind]);
        }
        else
        {
            printHelp (argv[0]);
	    std::exit(0);
        }
    }

    // perform some extra validation on the inputs
    this->validateAssets ();
    this->validateScreenshot ();

    // setup application state
    this->state.general.keepRunning = true;
    this->state.audio.enabled = this->settings.audio.enabled;
    this->state.audio.volume = this->settings.audio.volume;
}

std::filesystem::path CApplicationContext::translateBackground (const std::string& bgIdOrPath)
{
    if (bgIdOrPath.find ('/') == std::string::npos)
        return Steam::FileSystem::workshopDirectory (WORKSHOP_APP_ID, bgIdOrPath);

    return bgIdOrPath;
}

void CApplicationContext::validateAssets ()
{
    if (!this->settings.general.assets.empty ())
    {
        sLog.out ("Using wallpaper engine's assets at ", this->settings.general.assets, " based on --assets-dir parameter");
        return;
    }

    try
    {
        this->settings.general.assets = Steam::FileSystem::appDirectory (APP_DIRECTORY, "assets");
    }
    catch (std::runtime_error&)
    {
        // set current path as assets' folder
        std::filesystem::path directory = std::filesystem::canonical ("/proc/self/exe").parent_path () / "assets";
    }
}

void CApplicationContext::validateScreenshot ()
{
    if (!this->settings.screenshot.take)
        return;

    if (!this->settings.screenshot.path.has_extension ())
        sLog.exception ("Cannot determine screenshot format");

    std::string extension = this->settings.screenshot.path.extension ();

    if (extension == ".bmp")
        this->settings.screenshot.format = FIF_BMP;
    else if (extension == ".png")
        this->settings.screenshot.format = FIF_PNG;
    else if (extension == ".jpg" || extension == ".jpeg")
        this->settings.screenshot.format = FIF_JPEG;
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
    sLog.out ("\t--noautomute\t\t\t\tDisables the automute when an app is playing sound");
    sLog.out ("\t--screen-root <screen name>\tDisplay as screen's background");
    sLog.out ("\t--window <geometry>\tRuns in window mode, geometry has to be XxYxWxH and sets the position and size of the window");
    sLog.out ("\t--fps <maximum-fps>\t\t\tLimits the FPS to the given number, useful to keep battery consumption low");
    sLog.out ("\t--assets-dir <path>\t\t\tFolder where the assets are stored");
    sLog.out ("\t--screenshot\t\t\t\tTakes a screenshot of the background");
    sLog.out ("\t--list-properties\t\t\tList all the available properties and their possible values");
    sLog.out ("\t--set-property <name=value>\tOverrides the default value of the given property");
    sLog.out ("\t--no-fullscreen-pause\tPrevents the background pausing when an app is fullscreen");
    sLog.out ("\t--disable-mouse\tDisables mouse interactions");
}
