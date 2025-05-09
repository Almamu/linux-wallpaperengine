#include "CApplicationContext.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Logging/CLog.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <argparse/argparse.hpp>

#define WORKSHOP_APP_ID 431960
#define APP_DIRECTORY "wallpaper_engine"

using namespace WallpaperEngine::Application;

CApplicationContext::CApplicationContext (int argc, char* argv []) :
    m_argc (argc),
    m_argv (argv) {
    std::string lastScreen;

    argparse::ArgumentParser program ("linux-wallpaperengine", "0.0", argparse::default_arguments::help);

    auto& backgroundGroup = program.add_group ("Background options");
        auto& backgroundMode = backgroundGroup.add_mutually_exclusive_group (false);

        backgroundGroup.add_argument ("background id")
            .help ("The background to use as default for screens with no background specified")
            .action([this](const std::string& value) -> void {
                this->settings.general.defaultBackground = translateBackground (value);
            });

        backgroundMode.add_argument ("-w", "--window")
            .help ("Window geometry to use for the given screen")
            .action ([this](const std::string& value) -> void {
                if (this->settings.render.mode == DESKTOP_BACKGROUND) {
                    sLog.exception ("Cannot run in both background and window mode");
                }
                if (this->settings.render.mode == EXPLICIT_WINDOW) {
                    sLog.exception ("Only one window at a time can be specified in explicit window mode");
                }

                this->settings.render.mode = EXPLICIT_WINDOW;

                if (value.empty ()) {
                    sLog.exception ("Window geometry cannot be empty");
                }

                const char* str = value.c_str ();
                const char* delim1 = strchr (str, 'x');
                const char* delim2 = delim1 ? strchr (delim1, 'x') : nullptr;
                const char* delim3 = delim2 ? strchr (delim2, 'x') : nullptr;

                if (delim1 == nullptr || delim2 == nullptr || delim3 == nullptr) {
                    sLog.exception ("Window geometry must be in the format: XxYxWxH");
                }

                this->settings.render.window.geometry.x = strtol(str, nullptr, 10);
                this->settings.render.window.geometry.y = strtol (delim1 + 1, nullptr, 10);
                this->settings.render.window.geometry.z = strtol (delim2 + 1, nullptr, 10);
                this->settings.render.window.geometry.w = strtol (delim3 + 1, nullptr, 10);
            })
            .append ();
        backgroundMode.add_argument ("-r", "--screen-root")
            .help ("The screen the following settings will have an effect on")
            .action([this, &lastScreen](const std::string& value) -> void {
                if (this->settings.general.screenBackgrounds.find (value) != this->settings.general.screenBackgrounds.end ()) {
                    sLog.exception ("Cannot specify the same screen more than once: ", value);
                }
                if (this->settings.render.mode == EXPLICIT_WINDOW) {
                    sLog.exception ("Cannot run in both background and window mode");
                }

                this->settings.render.mode = DESKTOP_BACKGROUND;
                lastScreen = value;
                this->settings.general.screenBackgrounds [lastScreen] = "";
                this->settings.general.screenScalings [lastScreen] = this->settings.render.window.scalingMode;
                this->settings.general.screenClamps [lastScreen] = this->settings.render.window.clamp;
            })
            .append ();
        backgroundGroup.add_argument ("-b", "--bg")
            .help ("After --screen-root, specifies the background to use for the given screen")
            .action ([this, &lastScreen](const std::string& value) -> void {
                this->settings.general.screenBackgrounds [lastScreen] = translateBackground (value);
                // set the default background to the last one used
                this->settings.general.defaultBackground = translateBackground (value);
            })
            .append ();
        backgroundGroup.add_argument ("--scaling")
            .help ("Scaling mode to use when rendering the background, this applies to the previous --window or --screen-root output, or the default background if no other background is specified")
            .choices ("stretch", "fit", "fill", "default")
            .action([this, &lastScreen](const std::string& value) -> void {
                WallpaperEngine::Render::CWallpaperState::TextureUVsScaling mode;

                if (value == "stretch") {
                    mode = WallpaperEngine::Render::CWallpaperState::TextureUVsScaling::StretchUVs;
                } else if (value == "fit") {
                    mode = WallpaperEngine::Render::CWallpaperState::TextureUVsScaling::ZoomFitUVs;
                } else if (value == "fill") {
                    mode = WallpaperEngine::Render::CWallpaperState::TextureUVsScaling::ZoomFillUVs;
                } else if (value == "default") {
                    mode = WallpaperEngine::Render::CWallpaperState::TextureUVsScaling::DefaultUVs;
                } else {
                    sLog.exception ("Invalid scaling mode: ", value);
                }

                if (this->settings.render.mode == DESKTOP_BACKGROUND) {
                    this->settings.general.screenScalings [lastScreen] = mode;
                } else {
                    this->settings.render.window.scalingMode = mode;
                }
            })
            .append ();
        backgroundGroup.add_argument ("--clamp")
            .help ("Clamp mode to use when rendering the background, this applies to the previous --window or --screen-root output, or the default background if no other background is specified")
            .choices("clamp", "border", "repeat")
            .action([this, &lastScreen](const std::string& value) -> void {
                WallpaperEngine::Assets::ITexture::TextureFlags flags;

                if (value == "clamp") {
                    flags = WallpaperEngine::Assets::ITexture::TextureFlags::ClampUVs;
                } else if (value == "border") {
                    flags = WallpaperEngine::Assets::ITexture::TextureFlags::ClampUVsBorder;
                } else if (value == "repeat") {
                    flags = WallpaperEngine::Assets::ITexture::TextureFlags::NoFlags;
                } else {
                    sLog.exception ("Invalid clamp mode: ", value);
                }

                if (this->settings.render.mode == DESKTOP_BACKGROUND) {
                    this->settings.general.screenClamps [lastScreen] = flags;
                } else {
                    this->settings.render.window.clamp = flags;
                }
            });

    auto& performanceGroup = program.add_group ("Performance options");

        performanceGroup.add_argument ("-f", "--fps")
            .help ("Limits the FPS to the given number, useful to keep battery consumption low")
            .default_value (30)
            .store_into(this->settings.render.maximumFPS);

        performanceGroup.add_argument ("--no-fullscreen-pause")
            .help ("Prevents the background pausing when an app is fullscreen")
            .flag ()
            .action ([this](const std::string& value) -> void {
                this->settings.render.pauseOnFullscreen = false;
            });

    auto& audioGroup = program.add_group ("Sound settings");
    auto& audioSettingsGroup = audioGroup.add_mutually_exclusive_group (false);

        audioSettingsGroup.add_argument ("-v", "--volume")
            .help ("Volume for all the sounds in the background")
            .default_value (15)
            .store_into (this->settings.audio.volume);

        audioSettingsGroup.add_argument ("-s", "--silent")
            .help ("Mutes all the sound the wallpaper might produce")
            .flag ()
            .action ([this](const std::string& value) -> void {
                this->settings.audio.enabled = false;
            });

        audioGroup.add_argument ("--noautomute")
            .help ("Disables the automute when an app is playing sound")
            .flag ()
            .action([this](const std::string& value) -> void {
                this->settings.audio.automute = false;
            });

        audioGroup.add_argument ("--no-audio-processing")
            .help ("Disables audio processing for backgrounds")
            .flag ()
            .action ([this](const std::string& value) -> void {
                this->settings.audio.audioprocessing = false;
            });

    auto& screenshotGroup = program.add_group ("Screenshot options");

        screenshotGroup.add_argument ("--screenshot")
            .help ("Takes a screenshot of the background for it's use with tools like PyWAL")
            .default_value ("")
            .action ([this](const std::string& value) -> void {
                this->settings.screenshot.take = true;
                this->settings.screenshot.path = value;
            });

        screenshotGroup.add_argument ("--screenshot-delay")
            .help ("Frames to wait before taking the screenshot")
            .default_value (5)
            .store_into (this->settings.screenshot.delay);

    auto& contentGroup = program.add_group ("Content options");

        contentGroup.add_argument ("--assets-dir")
            .help ("Folder where the assets are stored")
            .default_value ("")
            .action ([this](const std::string& value) -> void {
                this->settings.general.assets = value;
            });

    auto& configurationGroup = program.add_group ("Wallpaper configuration options");

        configurationGroup.add_argument ("--disable-mouse")
            .help ("Disables mouse interaction with the backgrounds")
            .flag ()
            .action ([this](const std::string& value) -> void {
                this->settings.mouse.enabled = false;
            });

        configurationGroup.add_argument ("-l", "--list-properties")
            .help ("List all the available properties and their configuration")
            .flag ()
            .store_into (this->settings.general.onlyListProperties);

        configurationGroup.add_argument ("--set-property", "--property")
            .help ("Overrides the default value of the given property")
            .action([this](const std::string& value) -> void {
                const std::string::size_type equals = value.find ('=');

                // properties without value are treated as booleans for now
                if (equals == std::string::npos)
                    this->settings.general.properties [value] = "1";
                else
                    this->settings.general.properties [value.substr (0, equals)] = value.substr (equals + 1);
            });

    auto& debuggingGroup = program.add_group ("Debugging options");

        debuggingGroup.add_argument ("-z", "--dump-structure")
            .help ("Dumps the structure of the backgrounds")
            .flag ()
            .store_into (this->settings.general.dumpStructure);

    program.add_epilog (
        "Usage examples:\n"
            "  linux-wallpaperengine --screen-root HDMI-1 --bg 2317494988 --scaling fill --clamp border\n"
            "    Runs the background 2317494988 on screen HDMI-1, scaling it to fill the screen and clamping the UVs to the border\n\n"
            "  linux-wallpaperengine 2317494988\n"
            "    Previews the background 2317494988 on a window\n\n"
            "  linux-wallpaperengine --screen-root HDMI-1 --bg 2317494988 --screen-root HDMI-2 --bg 1108150151\n"
            "    Runs two backgrounds on two screens, one on HDMI-1 and the other on HDMI-2\n\n"
            "  linux-wallpaperengine --screen-root HDMI-1 --screen-root HDMI-2 2317494988\n"
            "    Runs the background 2317494988 on two screens, one on HDMI-1 and the other on HDMI-2\n\n"
    );

    program.parse_known_args (argc, argv);

    this->settings.audio.volume = std::max(0, std::min (this->settings.audio.volume, 128));
    this->settings.screenshot.delay = std::max (0, std::min (this->settings.screenshot.delay, 5));

    // use std::cout on this in case logging is disabled, this way it's easy to look at what is running
    std::stringbuf buffer;
    std::ostream bufferStream (&buffer);

    bufferStream << "Running with: ";

    for (int i = 0; i < argc; i ++) {
        bufferStream << argv [i];
        bufferStream << " ";
    }

    std::cout << buffer.str() << std::endl;
    // perform some extra validation on the inputs
    this->validateAssets ();
    this->validateScreenshot ();

    // setup application state
    this->state.general.keepRunning = true;
    this->state.audio.enabled = this->settings.audio.enabled;
    this->state.audio.volume = this->settings.audio.volume;
    this->state.mouse.enabled = this->settings.mouse.enabled;
}

int CApplicationContext::getArgc () const {
    return this->m_argc;
}

char** CApplicationContext::getArgv () const {
    return this->m_argv;
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

void CApplicationContext::validateScreenshot () const {
    if (!this->settings.screenshot.take)
        return;

    if (!this->settings.screenshot.path.has_extension ())
        sLog.exception ("Cannot determine screenshot format");

    const std::string extension = this->settings.screenshot.path.extension ();

    if (extension != ".bmp" && extension != ".png" && extension != ".jpeg" && extension != ".jpg")
        sLog.exception ("Cannot determine screenshot format, unknown extension ", extension);
}
