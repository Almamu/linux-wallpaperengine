#include <csignal>
#include <iostream>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Debugging/CallStack.h"
#include "WallpaperEngine/Logging/Log.h"

#include <linux-wallpaperengine/configuration.h>

WallpaperEngine::Application::WallpaperApplication* app;

void signalhandler (const int sig) {
	if (app == nullptr) {
		return;
	}

	app->signal (sig);
}

void initLogging () {
	sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
	sLog.addError (new std::ostream (std::cerr.rdbuf ()));
}

int main (int argc, char* argv[]) {
	// try {
	sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
	sLog.addError (new std::ostream (std::cerr.rdbuf ()));

	wp_configuration* config = wp_config_create ();

	WallpaperEngine::Application::ApplicationContext appContext (argc, argv, config);

	appContext.loadSettingsFromArgv ();

	// setup any of the specified options and create the context
	wp_config_set_web_fps_limit (config, appContext.settings.render.maximumFPS);
	wp_config_enable_audio (config, appContext.settings.audio.enabled);
	wp_config_set_audio_volume (config, appContext.settings.audio.volume);
	wp_config_set_disable_particles (config, appContext.settings.general.disableParticles);
	wp_config_set_disable_parallax (config, appContext.settings.mouse.disableparallax);
	// TODO: wp_config_set_mute_check & wp_config_set_rendering_pause_check

	app = new WallpaperEngine::Application::WallpaperApplication (appContext);

	// halt if the list-properties option was specified
	if (appContext.settings.general.onlyListProperties) {
		delete app;
		return 0;
	}

	// attach signals to gracefully stop
	std::signal (SIGINT, signalhandler);
	std::signal (SIGTERM, signalhandler);
	std::signal (SIGKILL, signalhandler);

	// show the wallpaper application
	app->show ();

	// remove signal handlers before destroying app
	std::signal (SIGINT, SIG_DFL);
	std::signal (SIGTERM, SIG_DFL);
	std::signal (SIGKILL, SIG_DFL);

	delete app;

	wp_config_destroy (config);

	return 0;
	/*} catch (const std::exception& e) {
	    std::cerr << e.what () << std::endl;
	    return 1;
	}*/
}