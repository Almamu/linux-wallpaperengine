#include "CApplicationContext.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Logging/CLog.h"

#include <cstdlib>
#include <cstring>
#include <getopt.h>

#define WORKSHOP_APP_ID 431960
#define APP_DIRECTORY "wallpaper_engine"

using namespace WallpaperEngine::Application;

struct option long_options [] = {
    {"screen-root", required_argument, nullptr, 'r'},   {"bg", required_argument, nullptr, 'b'},
    {"window", required_argument, nullptr, 'w'},        {"pkg", required_argument, nullptr, 'p'},
    {"dir", required_argument, nullptr, 'd'},           {"silent", no_argument, nullptr, 's'},
    {"volume", required_argument, nullptr, 'v'},        {"help", no_argument, nullptr, 'h'},
    {"fps", required_argument, nullptr, 'f'},           {"assets-dir", required_argument, nullptr, 'a'},
    {"screenshot", required_argument, nullptr, 'c'},    {"list-properties", no_argument, nullptr, 'l'},
    {"set-property", required_argument, nullptr, 'o'},  {"noautomute", no_argument, nullptr, 'm'},
    {"no-audio-processing", no_argument, nullptr, 'g'}, {"no-fullscreen-pause", no_argument, nullptr, 'n'},
    {"disable-mouse", no_argument, nullptr, 'e'},       {"scaling", required_argument, nullptr, 't'},
    {"clamping", required_argument, nullptr, 't'},      {nullptr, 0, nullptr, 0}};

/* std::hash::operator() isn't constexpr, so it can't be used to get hash values as compile-time constants
 * So here is customHash. It skips all spaces, so hashes for " find " and "fi nd" are the same
 * Basicly got it from here: https://stackoverflow.com/questions/8317508/hash-function-for-a-string
 */
constexpr size_t customHash (const char* str) {
    constexpr size_t A = 54059;   /* a prime */
    constexpr size_t B = 76963;   /* another prime */
    constexpr size_t C = 86969;   /* yet another prime */
    constexpr size_t FIRSTH = 37; /* also prime */
    size_t hash = FIRSTH;
    while (*str) {
        if (*str != ' ') // Skip spaces
            hash = (hash * A) ^ (*str * B);
        ++str;
    }
    return hash % C;
}

std::string stringPathFixes (const std::string& s) {
    if (s.empty ())
        return s;

    std::string str (s);

    // remove single-quotes from the arguments
    if (str [0] == '\'' && str [str.size () - 1] == '\'')
        str.erase (str.size () - 1, 1).erase (0, 1);

    return std::move (str);
}

CApplicationContext::CApplicationContext (std::string& path, std::string& display) {
    // setup structs with sane default values for now
    this->settings = {
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
                .window =
                    {
                        .geometry = {},
                        .clamp = WallpaperEngine::Assets::ITexture::TextureFlags::ClampUVs,
                        .scalingMode = WallpaperEngine::Render::CWallpaperState::TextureUVsScaling::DefaultUVs,
                    },
            },
        .audio = {.enabled = true, .volume = 15, .automute = true, .audioprocessing = true},
        .mouse =
            {
                .enabled = true,
            },
        .screenshot =
            {
                .take = false,
                .path = "",
            },
    };

    int c;

    std::string lastScreen;

    // handle screen screen-root
    if (this->settings.general.screenBackgrounds.find (display) !=
      this->settings.general.screenBackgrounds.end ()) sLog.exception ("Cannot specify the same screen more than once: ", optarg);
    if (this->settings.render.mode == EXPLICIT_WINDOW)
      sLog.exception ("Cannot run in both background and window mode");

    this->settings.render.mode = DESKTOP_BACKGROUND;
    lastScreen = display;
    this->settings.general.screenBackgrounds [lastScreen] = "";
    this->settings.general.screenScalings [lastScreen] = this->settings.render.window.scalingMode;
     
    this->settings.general.defaultBackground = translateBackground (path);

    // perform some extra validation on the inputs
    this->validateAssets ();
    this->validateScreenshot ();

    // setup application state
    this->state.general.keepRunning = true;
    this->state.audio.enabled = this->settings.audio.enabled;
    this->state.audio.volume = this->settings.audio.volume;


} 

CApplicationContext::CApplicationContext (int argc, char* argv []) {
    // setup structs with sane default values for now
    this->settings = {
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
                .window =
                    {
                        .geometry = {},
                        .clamp = WallpaperEngine::Assets::ITexture::TextureFlags::ClampUVs,
                        .scalingMode = WallpaperEngine::Render::CWallpaperState::TextureUVsScaling::DefaultUVs,
                    },
            },
        .audio = {.enabled = true, .volume = 15, .automute = true, .audioprocessing = true},
        .mouse =
            {
                .enabled = true,
            },
        .screenshot =
            {
                .take = false,
                .path = "",
            },
    };

    int c;

    std::string lastScreen;

    while ((c = getopt_long (argc, argv, "b:r:p:d:shf:a:w:mnt:", long_options, nullptr)) != -1) {
        if (!lastScreen.empty()) {
          sLog.out(lastScreen + "\n"); 
        }
        switch (c) {

            case 'n': this->settings.render.pauseOnFullscreen = false; break;

            case 'b':
                if (lastScreen.empty ())
                    sLog.exception ("--bg has to go after a --screen-root argument");

                // no need to check for previous screen being in the list, as it's the only way for this variable
                // to have any value
                this->settings.general.screenBackgrounds [lastScreen] = translateBackground (optarg);
                this->settings.general.screenScalings [lastScreen] = this->settings.render.window.scalingMode;

                // update default background if not set
                if (this->settings.general.defaultBackground.empty()) {
                    this->settings.general.defaultBackground = translateBackground (optarg);
                }
                break;

            case 'o': {
                std::string value = optarg;
                const std::string::size_type equals = value.find ('=');

                // properties without value are treated as booleans for now
                if (equals == std::string::npos)
                    this->settings.general.properties [value] = "1";
                else
                    this->settings.general.properties [value.substr (0, equals)] = value.substr (equals + 1);
            } break;

            case 'l': this->settings.general.onlyListProperties = true; break;

            case 'r':
                if (this->settings.general.screenBackgrounds.find (optarg) !=
                    this->settings.general.screenBackgrounds.end ())
                    sLog.exception ("Cannot specify the same screen more than once: ", optarg);
                if (this->settings.render.mode == EXPLICIT_WINDOW)
                    sLog.exception ("Cannot run in both background and window mode");

                this->settings.render.mode = DESKTOP_BACKGROUND;
                lastScreen = optarg;
                this->settings.general.screenBackgrounds [lastScreen] = "";
                this->settings.general.screenScalings [lastScreen] = this->settings.render.window.scalingMode;
                break;

            case 'w':
                if (this->settings.render.mode == DESKTOP_BACKGROUND)
                    sLog.exception ("Cannot run in both background and window mode");

                if (optarg != nullptr) {
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

            case 's': this->settings.audio.enabled = false; break;

            case 'h':
                printHelp (argv [0]);
                std::exit (0);
                break;

            case 'f': this->settings.render.maximumFPS = atoi (optarg); break;

            case 'a': this->settings.general.assets = stringPathFixes (optarg); break;

            case 'v': this->settings.audio.volume = std::max (atoi (optarg), 128); break;

            case 'c':
                this->settings.screenshot.take = true;
                this->settings.screenshot.path = stringPathFixes (optarg);
                break;

            case 'm': this->settings.audio.automute = false; break;

            case 'g': this->settings.audio.audioprocessing = false; break;

            case 'e': this->settings.mouse.enabled = false; break;

            case 't': {
                size_t hash = customHash (optarg);
                // Use a switch statement with the hash
                switch (hash) {
                    // --scale options
                    case customHash ("stretch"):
                        this->settings.render.window.scalingMode =
                            WallpaperEngine::Render::CWallpaperState::TextureUVsScaling::StretchUVs;
                        break;
                    case customHash ("fit"):
                        this->settings.render.window.scalingMode =
                            WallpaperEngine::Render::CWallpaperState::TextureUVsScaling::ZoomFitUVs;
                        break;
                    case customHash ("fill"):
                        this->settings.render.window.scalingMode =
                            WallpaperEngine::Render::CWallpaperState::TextureUVsScaling::ZoomFillUVs;
                        break;
                    case customHash ("default"):
                        this->settings.render.window.scalingMode =
                            WallpaperEngine::Render::CWallpaperState::TextureUVsScaling::DefaultUVs;
                        break;
                    // --clamp options
                    case customHash ("clamp"):
                        this->settings.render.window.clamp = WallpaperEngine::Assets::ITexture::TextureFlags::ClampUVs;
                        break;
                    case customHash ("border"):
                        this->settings.render.window.clamp =
                            WallpaperEngine::Assets::ITexture::TextureFlags::ClampUVsBorder;
                        break;
                    case customHash ("repeat"):
                        this->settings.render.window.clamp = WallpaperEngine::Assets::ITexture::TextureFlags::NoFlags;
                        break;
                    default:
                        sLog.error ("Wrong argument:");
                        sLog.error (optarg);
                        sLog.exception ("Wrong argument provided for --scale or --clamp option.");
                        break;
                }
            } break;
            default: sLog.out ("Default on path parsing: ", optarg); break;
        }
    }

    if (this->settings.general.defaultBackground.empty ()) {
        if (optind < argc && strlen (argv [optind]) > 0) {
            this->settings.general.defaultBackground = translateBackground (argv [optind]);
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

std::filesystem::path CApplicationContext::translateBackground (const std::string& bgIdOrPath) {
    if (bgIdOrPath.find ('/') == std::string::npos)
        return Steam::FileSystem::workshopDirectory (WORKSHOP_APP_ID, bgIdOrPath);

    return bgIdOrPath;
}

void CApplicationContext::validateAssets () {
    if (!this->settings.general.assets.empty ()) {
        sLog.out ("Using wallpaper engine's assets at ", this->settings.general.assets,
                  " based on --assets-dir parameter");
        return;
    }

    try {
        this->settings.general.assets = Steam::FileSystem::appDirectory (APP_DIRECTORY, "assets");
    } catch (std::runtime_error&) {
        // set current path as assets' folder
        this->settings.general.assets = std::filesystem::canonical ("/proc/self/exe").parent_path () / "assets";
    }
}

void CApplicationContext::validateScreenshot () {
    if (!this->settings.screenshot.take)
        return;

    if (!this->settings.screenshot.path.has_extension ())
        sLog.exception ("Cannot determine screenshot format");

    const std::string extension = this->settings.screenshot.path.extension ();

    if (extension != ".bmp" && extension != ".png" && extension != ".jpeg" && extension != ".jpg")
        sLog.exception ("Cannot determine screenshot format, unknown extension ", extension);
}

void CApplicationContext::printHelp (const char* route) {
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
    sLog.out ("\t--no-audio-processing\t\t\t\tDisables audio processing for backgrounds");
    sLog.out ("\t--screen-root <screen name>\tDisplay as screen's background");
    sLog.out (
        "\t--window <geometry>\tRuns in window mode, geometry has to be XxYxWxH and sets the position and size of the window");
    sLog.out ("\t--fps <maximum-fps>\t\t\tLimits the FPS to the given number, useful to keep battery consumption low");
    sLog.out ("\t--assets-dir <path>\t\t\tFolder where the assets are stored");
    sLog.out ("\t--screenshot\t\t\t\tTakes a screenshot of the background");
    sLog.out ("\t--list-properties\t\t\tList all the available properties and their possible values");
    sLog.out ("\t--set-property <name=value>\tOverrides the default value of the given property");
    sLog.out ("\t--no-fullscreen-pause\tPrevents the background pausing when an app is fullscreen");
    sLog.out ("\t--disable-mouse\tDisables mouse interactions");
    sLog.out (
        "\t--bg <background_path/background_id>\tAfter --screen-root uses the specified background only on that screen");
    sLog.out (
        "\t--scaling <mode>\t Scaling mode for wallpaper. Can be stretch, fit, fill, default. Must be used before wallpaper provided.\n\
                    \t\t For default wallpaper last specified value will be used.\n\
                    \t\t Example: ./wallengine --scaling stretch --screen-root eDP-1 --bg 2667198601 --scaling fill --screen-root eDP-2 2667198602");
    sLog.out (
        "\t--clamping <mode>\t Clamping mode for all wallpapers. Can be clamp, border, repeat. Enables GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER, GL_REPEAT accordingly. Default is clamp.");
}
