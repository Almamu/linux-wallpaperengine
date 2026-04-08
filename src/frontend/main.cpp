#include <csignal>
#include <iostream>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"

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
	try {
		sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
		sLog.addError (new std::ostream (std::cerr.rdbuf ()));

		WallpaperEngine::Application::ApplicationContext appContext (argc, argv);

		appContext.loadSettingsFromArgv ();

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

		return 0;
	} catch (const std::exception& e) {
		std::cerr << e.what () << std::endl;
		return 1;
	}
}