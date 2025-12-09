#include "ApplicationContext.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Data/JSON.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string_view>

#include <argparse/argparse.hpp>

#define WORKSHOP_APP_ID 431960
#define APP_DIRECTORY "wallpaper_engine"

using namespace WallpaperEngine::Application;
using WallpaperEngine::Data::JSON::JSON;

std::filesystem::path ApplicationContext::resolvePlaylistItemPath (const std::string& raw) const {
    if (raw.empty ())
        return {};

    std::string cleaned = raw;

    

    constexpr std::string_view windowsPrefix = "\\\\?\\";

    if (cleaned.rfind (windowsPrefix, 0) == 0)
        cleaned = cleaned.substr (windowsPrefix.length ());

    std::replace (cleaned.begin (), cleaned.end (), '\\', '/');

    if (cleaned.size () > 1 && cleaned[1] == ':')
        cleaned = cleaned.substr (2);

    if (!cleaned.empty () && cleaned.front () != '/')
        cleaned.insert (cleaned.begin (), '/');

    std::filesystem::path path = std::filesystem::path (cleaned).lexically_normal ();

    if (std::filesystem::is_regular_file (path))
        path = path.parent_path ();

    return path;
}

std::filesystem::path ApplicationContext::configFilePath () const {
    try {
        return Steam::FileSystem::appDirectory (APP_DIRECTORY, "") / "config.json";
    } catch (std::runtime_error&) {
        sLog.exception ("Cannot locate wallpaper engine installation to read config.json");
        return {};
    }
}

std::optional<JSON> ApplicationContext::parseConfigJson (const std::filesystem::path& path) const {
    if (path.empty ())
        return std::nullopt;

    std::ifstream configFile (path);

    if (!configFile.is_open ()) {
        sLog.exception ("Cannot open wallpaper engine config file at ", path);
        return std::nullopt;
    }

    try {
        return JSON::parse (configFile);
    } catch (const std::exception& e) {
        sLog.error ("Failed parsing wallpaper engine config.json: ", e.what ());
        return std::nullopt;
    }
}

std::optional<ApplicationContext::PlaylistDefinition>
ApplicationContext::buildPlaylistDefinition (const JSON& playlistJson, const std::string& fallbackName) const {
    PlaylistDefinition definition;
    definition.name = playlistJson.optional<std::string> ("name", fallbackName);
    definition.settings = this->parsePlaylistSettings (playlistJson);
    definition.items = this->collectPlaylistItems (playlistJson, definition.name.empty () ? fallbackName : definition.name);

    if (definition.items.empty ()) {
        return std::nullopt;
    }

    if (definition.name.empty ()) {
        if (fallbackName.empty ()) {
            sLog.error ("Skipping playlist with no name");
            return std::nullopt;
        }

        definition.name = fallbackName;
    }

    return definition;
}

ApplicationContext::PlaylistSettings ApplicationContext::parsePlaylistSettings (const JSON& playlistJson) const {
    PlaylistSettings settings;
    const auto settingsJson = playlistJson.optional ("settings");
    settings.delayMinutes = settingsJson ? settingsJson->optional<uint32_t> ("delay", 60) : 60;
    settings.mode = settingsJson ? settingsJson->optional<std::string> ("mode", "timer") : "timer";
    settings.order = settingsJson ? settingsJson->optional<std::string> ("order", "sequential") : "sequential";
    settings.updateOnPause = settingsJson ? settingsJson->optional<bool> ("updateonpause", false) : false;
    settings.videoSequence = settingsJson ? settingsJson->optional<bool> ("videosequence", false) : false;
    return settings;
}

std::vector<std::filesystem::path>
ApplicationContext::collectPlaylistItems (const JSON& playlistJson, const std::string& name) const {
    std::vector<std::filesystem::path> items;
    const auto jsonItems = playlistJson.optional ("items");

    if (!jsonItems.has_value () || !jsonItems->is_array ()) {
        sLog.error ("Skipping playlist ", name, ": missing items");
        return items;
    }

    for (const auto& rawItem : *jsonItems) {
        if (!rawItem.is_string ())
            continue;

        auto resolvedPath = this->resolvePlaylistItemPath (rawItem.get<std::string> ());

        if (resolvedPath.empty ())
            continue;

        if (!std::filesystem::exists (resolvedPath)) {
            sLog.error ("Skipping playlist item not found: ", resolvedPath.string ());
            continue;
        }

        items.push_back (resolvedPath);
    }

    if (items.empty ()) {
        sLog.error ("Skipping playlist ", name, ": no usable items found");
    }

    return items;
}

void ApplicationContext::registerPlaylist (PlaylistDefinition&& definition) {
    this->m_configPlaylists.insert_or_assign (definition.name, std::move (definition));
}

void ApplicationContext::loadPlaylistsFromConfig () {
    if (this->m_loadedConfigPlaylists)
        return;

    this->m_loadedConfigPlaylists = true;

    const auto configPath = this->configFilePath ();
    const auto root = this->parseConfigJson (configPath);
    if (!root.has_value ())
        return;

    const auto steamUser = root->optional ("steamuser");

    if (!steamUser.has_value ()) {
        sLog.exception ("Cannot find steamuser section in config.json");
    }

    auto addPlaylist = [this](const JSON& playlistJson, const std::string& fallbackName) {
        auto definition = this->buildPlaylistDefinition (playlistJson, fallbackName);
        if (!definition)
            return;
        this->registerPlaylist (std::move (*definition));
    };

    if (const auto general = steamUser->optional ("general")) {
        if (const auto playlists = general->optional ("playlists")) {
            for (const auto& playlist : *playlists) {
                try {
                    addPlaylist (playlist, playlist.optional<std::string> ("name", ""));
                } catch (const std::exception& e) {
                    sLog.error ("Failed parsing playlist: ", e.what ());
                }
            }
        }
    }

    if (const auto wallpaperConfig = steamUser->optional ("wallpaperconfig")) {
        if (const auto selected = wallpaperConfig->optional ("selectedwallpapers")) {
            for (const auto& entry : selected->items ()) {
                const auto playlist = entry.value ().optional ("playlist");

                if (!playlist.has_value ())
                    continue;

                try {
                    addPlaylist (*playlist, entry.key ());
                } catch (const std::exception& e) {
                    sLog.error ("Failed parsing playlist for ", entry.key (), ": ", e.what ());
                }
            }
        }
    }
}

const ApplicationContext::PlaylistDefinition&
ApplicationContext::getPlaylistFromConfig (const std::string& name) {
    if (!this->m_loadedConfigPlaylists) {
        this->loadPlaylistsFromConfig ();
    }

    const auto cur = this->m_configPlaylists.find (name);

    if (cur == this->m_configPlaylists.end ()) {
        std::string available;

        for (auto it = this->m_configPlaylists.begin (); it != this->m_configPlaylists.end (); ++it) {
            available += it->first;
            if (std::next (it) != this->m_configPlaylists.end ())
                available += ", ";
        }

        const std::string availableText = available.empty () ? "" : std::string (". Available: ") + available;

        sLog.exception ("Playlist not found in config.json: ", name, availableText);
        throw std::runtime_error ("Playlist not found: " + name);
    }

    return cur->second;
}

ApplicationContext::ApplicationContext (int argc, char* argv []) :
    m_argc (argc),
    m_argv (argv) {
    std::string lastScreen;

    argparse::ArgumentParser program ("linux-wallpaperengine", "0.0", argparse::default_arguments::help);

    auto& backgroundGroup = program.add_group ("Background options");
        auto& backgroundMode = backgroundGroup.add_mutually_exclusive_group (false);

        backgroundGroup.add_argument ("background id")
            .help ("The background to use as default for screens with no background specified")
            .default_value ("")
            .action([this](const std::string& value) -> void {
                if (!value.empty()) {
                    this->settings.general.defaultBackground = translateBackground (value);
                }
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
                const char* delim2 = delim1 ? strchr (delim1 + 1, 'x') : nullptr;
                const char* delim3 = delim2 ? strchr (delim2 + 1, 'x') : nullptr;

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
        backgroundGroup.add_argument ("--playlist")
            .help ("Uses a playlist from wallpaper engine's config.json. If used after --screen-root it is applied to that screen, otherwise it is used in window mode.")
            .action ([this, &lastScreen](const std::string& value) -> void {
                const auto& playlist = this->getPlaylistFromConfig (value);

                if (lastScreen.empty ()) {
                    this->settings.general.defaultPlaylist = playlist;
                    if (this->settings.general.defaultBackground.empty () && !playlist.items.empty ()) {
                        this->settings.general.defaultBackground = playlist.items.front ();
                    }
                } else {
                    this->settings.general.screenPlaylists [lastScreen] = playlist;
                    if (!playlist.items.empty ()) {
                        this->settings.general.screenBackgrounds [lastScreen] = playlist.items.front ();
                    }

                    if (this->settings.general.defaultBackground.empty () && !playlist.items.empty ()) {
                        this->settings.general.defaultBackground = playlist.items.front ();
                    }
                }
            })
            .append ();
        backgroundGroup.add_argument ("--scaling")
            .help ("Scaling mode to use when rendering the background, this applies to the previous --window or --screen-root output, or the default background if no other background is specified")
            .choices ("stretch", "fit", "fill", "default")
            .action([this, &lastScreen](const std::string& value) -> void {
                WallpaperEngine::Render::WallpaperState::TextureUVsScaling mode;

                if (value == "stretch") {
                    mode = WallpaperEngine::Render::WallpaperState::TextureUVsScaling::StretchUVs;
                } else if (value == "fit") {
                    mode = WallpaperEngine::Render::WallpaperState::TextureUVsScaling::ZoomFitUVs;
                } else if (value == "fill") {
                    mode = WallpaperEngine::Render::WallpaperState::TextureUVsScaling::ZoomFillUVs;
                } else if (value == "default") {
                    mode = WallpaperEngine::Render::WallpaperState::TextureUVsScaling::DefaultUVs;
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
                TextureFlags flags;

                if (value == "clamp") {
                    flags = TextureFlags_ClampUVs;
                } else if (value == "border") {
                    flags = TextureFlags_ClampUVsBorder;
                } else if (value == "repeat") {
                    flags = TextureFlags_NoFlags;
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
            .default_value <uint32_t> (5)
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
        configurationGroup.add_argument ("--disable-parallax")
            .help ("Disables parallax effect for the backgrounds")
            .flag ()
            .action ([this](const std::string& value) -> void {
                this->settings.mouse.disableparallax = true;
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
            })
            .append ();

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

    try {
        program.parse_known_args (argc, argv);

        if (this->settings.general.defaultBackground.empty ()) {
            throw std::runtime_error ("At least one background ID must be specified");
        }

        this->settings.audio.volume = std::max(0, std::min (this->settings.audio.volume, 128));
        this->settings.screenshot.delay = std::max <uint32_t> (0, std::min <uint32_t> (this->settings.screenshot.delay, 5));

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

#if DEMOMODE
        sLog.error ("WARNING: RUNNING IN DEMO MODE WILL STOP WALLPAPERS AFTER 5 SECONDS SO VIDEO CAN BE RECORDED");
        // special settings for demomode
        this->settings.render.maximumFPS = 30;
        this->settings.screenshot.take = false;
        this->settings.render.pauseOnFullscreen = false;
#endif /* DEMOMODE */
   } catch (const std::runtime_error& e) {
       throw std::runtime_error (std::string (e.what()) + ". Use " + std::string (argv[0]) + " --help for more information");
   }
}

int ApplicationContext::getArgc () const {
    return this->m_argc;
}

char** ApplicationContext::getArgv () const {
    return this->m_argv;
}

std::filesystem::path ApplicationContext::translateBackground (const std::string& bgIdOrPath) {
    if (bgIdOrPath.find ('/') == std::string::npos)
        return Steam::FileSystem::workshopDirectory (WORKSHOP_APP_ID, bgIdOrPath);

    return bgIdOrPath;
}

void ApplicationContext::validateAssets () {
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

void ApplicationContext::validateScreenshot () const {
    if (!this->settings.screenshot.take)
        return;

    if (!this->settings.screenshot.path.has_extension ())
        sLog.exception ("Cannot determine screenshot format");

    const std::string extension = this->settings.screenshot.path.extension ();

    if (extension != ".bmp" && extension != ".png" && extension != ".jpeg" && extension != ".jpg")
        sLog.exception ("Cannot determine screenshot format, unknown extension ", extension);
}
