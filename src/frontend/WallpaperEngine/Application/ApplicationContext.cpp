#include "ApplicationContext.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Logging/Log.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ranges>
#include <string_view>

#include <argparse/argparse.hpp>

#include <linux-wallpaperengine/playlists.h>

using namespace WallpaperEngine::Application;

ApplicationContext::ApplicationContext (int argc, char* argv[], wp_configuration* config) :
	m_argc (argc), m_argv (argv), m_config (config) { }

void ApplicationContext::loadSettingsFromArgv () {
	std::string lastScreen = DEFAULT_SCREEN_NAME;

	argparse::ArgumentParser program ("linux-wallpaperengine", "0.0", argparse::default_arguments::help);

	auto& backgroundGroup = program.add_group ("Background options");
	auto& backgroundMode = backgroundGroup.add_mutually_exclusive_group (false);

	backgroundGroup.add_argument ("background id")
		.help ("The background to use as default for screens with no background specified")
		.default_value ("")
		.action ([this] (const std::string& value) -> void {
			if (!value.empty ()) {
				this->settings.general.backgrounds[DEFAULT_SCREEN_NAME] = value;
			}
		});

	backgroundMode.add_argument ("-w", "--window")
		.help ("Window geometry to use for the given screen")
		.action ([this] (const std::string& value) -> void {
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

			this->settings.render.window.geometry.x = strtol (str, nullptr, 10);
			this->settings.render.window.geometry.y = strtol (delim1 + 1, nullptr, 10);
			this->settings.render.window.geometry.z = strtol (delim2 + 1, nullptr, 10);
			this->settings.render.window.geometry.w = strtol (delim3 + 1, nullptr, 10);
		})
		.append ();
	backgroundMode.add_argument ("-r", "--screen-root")
		.help ("The screen the following settings will have an effect on")
		.action ([this, &lastScreen] (const std::string& value) -> void {
			if (this->settings.general.backgrounds.contains (value)) {
				sLog.exception ("Cannot specify the same screen more than once: ", value);
			}
			if (this->settings.render.mode == EXPLICIT_WINDOW) {
				sLog.exception ("Cannot run in both background and window mode");
			}

			this->settings.render.mode = DESKTOP_BACKGROUND;
			lastScreen = value;
			this->settings.general.backgrounds[lastScreen] = "";
			this->settings.general.screenScalings[lastScreen] = this->settings.render.window.scalingMode;
			this->settings.general.screenClamps[lastScreen] = this->settings.render.window.clamp;
		})
		.append ();
	backgroundGroup.add_argument ("-b", "--bg")
		.help ("After --screen-root, specifies the background to use for the given screen")
		.action ([this, &lastScreen] (const std::string& value) -> void {
			this->settings.general.backgrounds[lastScreen] = value;
		})
		.append ();
	backgroundGroup.add_argument ("--playlist")
		.help (
			"Uses a playlist from wallpaper engine's config.json. If used after --screen-root it is applied to that "
			"screen, otherwise it is used in window mode."
		)
		.action ([this, &lastScreen] (const std::string& value) -> void {
			this->settings.general.playlists[lastScreen] = value;
		})
		.append ();
	backgroundGroup.add_argument ("--scaling")
		.help (
			"Scaling mode to use when rendering the background, this applies to the previous --window or --screen-root "
			"output, or the default background if no other background is specified"
		)
		.choices ("stretch", "fit", "fill", "default")
		.action ([this, &lastScreen] (const std::string& value) -> void {
			SCALING_MODE mode;

			if (value == "stretch") {
				mode = SCALING_MODE_STRETCH;
			} else if (value == "fit") {
				mode = SCALING_MODE_FIT;
			} else if (value == "fill") {
				mode = SCALING_MODE_FILL;
			} else if (value == "default") {
				mode = SCALING_MODE_DEFAULT;
			} else {
				sLog.exception ("Invalid scaling mode: ", value);
			}

			if (this->settings.render.mode == DESKTOP_BACKGROUND) {
				this->settings.general.screenScalings[lastScreen] = mode;
			} else {
				this->settings.render.window.scalingMode = mode;
			}
		})
		.append ();
	backgroundGroup.add_argument ("--clamp")
		.help (
			"Clamp mode to use when rendering the background, this applies to the previous --window or --screen-root "
			"output, or the default background if no other background is specified"
		)
		.choices ("clamp", "border", "repeat")
		.action ([this, &lastScreen] (const std::string& value) -> void {
			CLAMP_MODE mode;

			if (value == "clamp") {
				mode = CLAMP_MODE_UVS;
			} else if (value == "border") {
				mode = CLAMP_MODE_UVS_BORDER;
			} else if (value == "repeat") {
				mode = CLAMP_MODE_REPEAT;
			} else {
				sLog.exception ("Invalid clamp mode: ", value);
			}

			if (this->settings.render.mode == DESKTOP_BACKGROUND) {
				this->settings.general.screenClamps[lastScreen] = mode;
			} else {
				this->settings.render.window.clamp = mode;
			}
		});

	auto& performanceGroup = program.add_group ("Performance options");

	performanceGroup.add_argument ("-f", "--fps")
		.help ("Limits the FPS to the given number, useful to keep battery consumption low")
		.default_value (30)
		.store_into (this->settings.render.maximumFPS);

	performanceGroup.add_argument ("--no-fullscreen-pause")
		.help ("Prevents the background pausing when an app is fullscreen")
		.flag ()
		.action ([this] (const std::string& value) -> void { this->settings.render.pauseOnFullscreen = false; });

	performanceGroup.add_argument ("--fullscreen-pause-only-active")
		.help ("Wayland only: pause only when a fullscreen window is active (activated)")
		.flag ()
		.action ([this] (const std::string& value) -> void {
			this->settings.render.pauseOnFullscreenOnlyWhenActive = true;
		});

	performanceGroup.add_argument ("--fullscreen-pause-ignore-appid")
		.help ("Wayland only: ignore fullscreen windows whose app_id contains this value (repeatable)")
		.action ([this] (const std::string& value) -> void {
			if (!value.empty ()) {
				this->settings.render.fullscreenPauseIgnoreAppIds.push_back (value);
			}
		})
		.append ();

	auto& audioGroup = program.add_group ("Sound settings");
	auto& audioSettingsGroup = audioGroup.add_mutually_exclusive_group (false);

	audioSettingsGroup.add_argument ("-v", "--volume")
		.help ("Volume for all the sounds in the background")
		.default_value (15)
		.store_into (this->settings.audio.volume);

	audioSettingsGroup.add_argument ("-s", "--silent")
		.help ("Mutes all the sound the wallpaper might produce")
		.flag ()
		.action ([this] (const std::string& value) -> void { this->settings.audio.enabled = false; });

	audioGroup.add_argument ("--noautomute")
		.help ("Disables the automute when an app is playing sound")
		.flag ()
		.action ([this] (const std::string& value) -> void { this->settings.audio.automute = false; });

	audioGroup.add_argument ("--no-audio-processing")
		.help ("Disables audio processing for backgrounds")
		.flag ()
		.action ([this] (const std::string& value) -> void { this->settings.audio.audioprocessing = false; });

	auto& screenshotGroup = program.add_group ("Screenshot options");

	screenshotGroup.add_argument ("--screenshot")
		.help ("Takes a screenshot of the background for it's use with tools like PyWAL")
		.default_value ("")
		.action ([this] (const std::string& value) -> void {
			this->settings.screenshot.take = true;
			this->settings.screenshot.path = value;
		});

	screenshotGroup.add_argument ("--screenshot-delay")
		.help ("Frames to wait before taking the screenshot")
		.default_value<uint32_t> (5)
		.store_into (this->settings.screenshot.delay);

	auto& contentGroup = program.add_group ("Content options");

	contentGroup.add_argument ("--assets-dir")
		.help ("Folder where the assets are stored")
		.default_value ("")
		.action ([this] (const std::string& value) -> void {
			if (this->settings.general.steam.has_value ()) {
				sLog.exception ("Only one of --assets-dir or --steam-dir can be specified");
			}

			this->settings.general.assets = value;
		});
	contentGroup.add_argument ("--steam-dir")
		.help ("Base folder to the steam installation (where steamapps exists)")
		.default_value ("")
		.action ([this] (const std::string& value) -> void {
			if (this->settings.general.assets.has_value ()) {
				sLog.exception ("Only one of --assets-dir or --steam-dir can be specified");
			}

			this->settings.general.steam = value;
		});

	auto& configurationGroup = program.add_group ("Wallpaper configuration options");

	configurationGroup.add_argument ("--disable-particles")
		.help ("Disables particles for the backgrounds")
		.flag ()
		.action ([this] (const std::string& value) -> void { this->settings.general.disableParticles = true; });

	configurationGroup.add_argument ("--disable-mouse")
		.help ("Disables mouse interaction with the backgrounds")
		.flag ()
		.action ([this] (const std::string& value) -> void { this->settings.mouse.enabled = false; });
	configurationGroup.add_argument ("--disable-parallax")
		.help ("Disables parallax effect for the backgrounds")
		.flag ()
		.action ([this] (const std::string& value) -> void { this->settings.mouse.disableparallax = true; });

	configurationGroup.add_argument ("-l", "--list-properties")
		.help ("List all the available properties and their configuration")
		.flag ()
		.store_into (this->settings.general.onlyListProperties);

	configurationGroup.add_argument ("--set-property", "--property")
		.help ("Overrides the default value of the given property")
		.action ([this] (const std::string& value) -> void {
			const std::string::size_type equals = value.find ('=');

			// properties without value are treated as booleans for now
			if (equals == std::string::npos) {
				this->settings.general.properties[value] = "1";
			} else {
				this->settings.general.properties[value.substr (0, equals)] = value.substr (equals + 1);
			}
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
		"    Runs the background 2317494988 on screen HDMI-1, scaling it to fill the screen and clamping the UVs to "
		"the border\n\n"
		"  linux-wallpaperengine 2317494988\n"
		"    Previews the background 2317494988 on a window\n\n"
		"  linux-wallpaperengine --screen-root HDMI-1 --bg 2317494988 --screen-root HDMI-2 --bg 1108150151\n"
		"    Runs two backgrounds on two screens, one on HDMI-1 and the other on HDMI-2\n\n"
		"  linux-wallpaperengine --screen-root HDMI-1 --screen-root HDMI-2 2317494988\n"
		"    Runs the background 2317494988 on two screens, one on HDMI-1 and the other on HDMI-2\n\n"
	);

	try {
		program.parse_known_args (this->m_argc, this->m_argv);

		if (this->settings.general.backgrounds.empty ()) {
			sLog.exception ("At least one background ID must be specified");
		}

		this->settings.audio.volume = std::max (0, std::min (this->settings.audio.volume, 128));
		this->settings.screenshot.delay
			= std::max<uint32_t> (0, std::min<uint32_t> (this->settings.screenshot.delay, 5));

		// use std::cout on this in case logging is disabled, this way it's easy to look at what is running
		std::stringbuf buffer;
		std::ostream bufferStream (&buffer);

		bufferStream << "Running with: ";

		for (int i = 0; i < this->m_argc; i++) {
			bufferStream << this->m_argv[i];
			bufferStream << " ";
		}

		std::cout << buffer.str () << std::endl;
		// perform some extra validation on the inputs
		this->validateAssets ();
		this->validateScreenshot ();
#if DEMOMODE
		sLog.error ("WARNING: RUNNING IN DEMO MODE WILL STOP WALLPAPERS AFTER 5 SECONDS SO VIDEO CAN BE RECORDED");
		// special settings for demomode
		this->settings.render.maximumFPS = 30;
		this->settings.screenshot.take = false;
		this->settings.render.pauseOnFullscreen = false;
#endif /* DEMOMODE */
	} catch (const std::runtime_error& e) {
		throw std::runtime_error (
			std::string (e.what ()) + ". Use " + std::string (this->m_argv[0]) + " --help for more information"
		);
	}
}

int ApplicationContext::getArgc () const { return this->m_argc; }

char** ApplicationContext::getArgv () const { return this->m_argv; }

wp_configuration* ApplicationContext::getConfig () const { return this->m_config; }

const std::map<std::string, ApplicationContext::PlaylistDefinition>& ApplicationContext::getPlaylists () const {
	return this->m_playlists;
}

void ApplicationContext::validateAssets () const {
	bool directoryFound = false;

	// try with assets, then with steam directory
	if (this->settings.general.assets.has_value ()) {
		directoryFound = wp_config_set_assets_dir (this->m_config, this->settings.general.assets.value ().c_str ());

		if (directoryFound == false) {
			sLog.error (
				"Assets directory specified is invalid or inaccessible: ", this->settings.general.assets.value ()
			);
		}
	}

	if (this->settings.general.steam.has_value ()) {
		directoryFound = directoryFound
			|| wp_config_set_steam_dir (this->m_config, this->settings.general.steam.value ().c_str ());

		if (directoryFound == false) {
			sLog.error (
				"Steam directory specified is invalid or inaccessible: ", this->settings.general.steam.value ()
			);
		}
	}

	// otherwise try the steam directory detection
	directoryFound = directoryFound || wp_config_detect_steam_dir (this->m_config);

	if (directoryFound == false) {
		sLog.error ("Could not automatically detect steam directory");
	}
	// last resort, use assets dir from current path
	directoryFound
		= directoryFound
		|| wp_config_set_assets_dir (
			  this->m_config, (std::filesystem::canonical ("/proc/self/exe").parent_path () / "assets").c_str ()
		);

	if (directoryFound == false) {
		sLog.exception ("Could not automatically detect assets or steam directory");
	}
}

void ApplicationContext::validateScreenshot () const {
	if (!this->settings.screenshot.take) {
		return;
	}

	if (!this->settings.screenshot.path.has_extension ()) {
		sLog.exception ("Cannot determine screenshot format");
	}

	const std::string extension = this->settings.screenshot.path.extension ();

	if (extension != ".bmp" && extension != ".png" && extension != ".jpeg" && extension != ".jpg") {
		sLog.exception ("Cannot determine screenshot format, unknown extension ", extension);
	}
}

void ApplicationContext::validatePlaylists () {
	std::vector<std::string> playlistsToCheck;

	if (this->settings.general.playlists.contains (DEFAULT_SCREEN_NAME)) {
		playlistsToCheck.push_back (this->settings.general.playlists[DEFAULT_SCREEN_NAME]);
	}

	for (const auto& playlist : this->settings.general.playlists | std::views::values) {
		playlistsToCheck.push_back (playlist);
	}

	wp_playlists* playlists = wp_playlists_load (this->m_config);

	for (wp_playlist_entry* entry = wp_playlists_next (playlists); entry != nullptr;
	     entry = wp_playlists_next (playlists)) {
		const auto count = std::erase_if (playlistsToCheck, [entry] (const std::string& playlist) {
			return playlist == entry->name;
		});

		if (count == 0) {
			continue;
		}

		auto inserted = this->m_playlists.emplace (
			entry->name,
			PlaylistDefinition { .name = entry->name,
		                         .items = {},
		                         .settings = PlaylistSettings { .delayMinutes = entry->delay,
		                                                        .mode = entry->mode,
		                                                        .order = entry->order,
		                                                        .updateOnPause = false,
		                                                        .videoSequence = false } }
		);

		if (inserted.second == false) {
			// push the playlist back again, could not be added to the list of playlists available
			playlistsToCheck.push_back (entry->name);
		}

		// add all the entries to the playlist
		for (int i = 0; i < entry->item_count; i++) {
			inserted.first->second.items.push_back (entry->items[i]);
		}
	}

	wp_playlists_destroy (playlists);

	if (playlistsToCheck.size () > 0) {
		sLog.error ("Could not find the following playlists: ");

		for (const auto& playlist : playlistsToCheck) {
			sLog.error (playlist);
		}

		sLog.exception ();
	}
}