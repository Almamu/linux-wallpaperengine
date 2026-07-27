#include <csignal>
#include <iostream>

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "WallpaperEngine/Logging/Log.h"
#include "include/cef_app.h"
#include "include/cef_command_line.h"

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

// Minimal CEF app used when this binary is re-exec'd as a Chromium child
// (--type=utility/gpu/renderer/...). Going through full WallpaperApplication
// first breaks those children (arg parse failures / crash loops) so network
// and GPU services never stay up — web wallpapers stay black.
//
// Custom schemes must match the browser process (SubprocessApp). BrowserApp
// passes them via --wp-schemes=id1,id2 on child launches.
class EarlyCefSubprocessApp : public CefApp {
public:
    void OnRegisterCustomSchemes (CefRawPtr<CefSchemeRegistrar> registrar) override {
	const CefRefPtr<CefCommandLine> commandLine = CefCommandLine::GetGlobalCommandLine ();
	if (!commandLine || !commandLine->HasSwitch ("wp-schemes")) {
	    return;
	}
	const std::string schemes = commandLine->GetSwitchValue ("wp-schemes").ToString ();
	std::size_t start = 0;
	while (start < schemes.size ()) {
	    const std::size_t comma = schemes.find (',', start);
	    const std::string id = schemes.substr (
		start, comma == std::string::npos ? std::string::npos : comma - start
	    );
	    if (!id.empty ()) {
		// Keep flags in sync with SubprocessApp::OnRegisterCustomSchemes.
		registrar->AddCustomScheme (
		    "wp" + id,
		    CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE | CEF_SCHEME_OPTION_FETCH_ENABLED
		);
	    }
	    if (comma == std::string::npos) {
		break;
	    }
	    start = comma + 1;
	}
    }

private:
    IMPLEMENT_REFCOUNTING (EarlyCefSubprocessApp);
};

int main (int argc, char* argv[]) {
    try {
	// Hand off to CEF immediately for Chromium child processes.
	{
	    const CefMainArgs main_args (argc, argv);
	    CefRefPtr<CefCommandLine> commandLine = CefCommandLine::CreateCommandLine ();
	    commandLine->InitFromArgv (argc, argv);
	    if (commandLine->HasSwitch ("type")) {
		CefRefPtr<CefApp> subprocessApp = new EarlyCefSubprocessApp ();
		const int exit_code = CefExecuteProcess (main_args, subprocessApp, nullptr);
		if (exit_code >= 0) {
		    return exit_code;
		}
	    }
	}

	// if type parameter is specified, this is a subprocess, so no logging should be enabled from our side
	bool enableLogging = true;
	const std::string typeZygote = "--type=zygote";
	const std::string typeUtility = "--type=utility";

	for (int i = 1; i < argc; i++) {
	    if (strncmp (typeZygote.c_str (), argv[i], typeZygote.size ()) == 0) {
		enableLogging = false;
		break;
	    }

	    if (strncmp (typeUtility.c_str (), argv[i], typeUtility.size ()) == 0) {
		enableLogging = false;
		break;
	    }
	}

	if (enableLogging) {
	    initLogging ();
	}

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