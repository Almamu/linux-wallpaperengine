#include "WebBrowserContext.h"
#include "CEF/BrowserApp.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/WebBrowser/CEF/SubprocessApp.h"
#include "include/cef_app.h"
#include "include/cef_render_handler.h"
#include <filesystem>
#include <random>

using namespace WallpaperEngine::WebBrowser;

// TODO: THIS IS USED TO GENERATE A RANDOM FOLDER FOR THE CHROME PROFILE, MAYBE A DIFFERENT APPROACH WOULD BE BETTER?
namespace uuid {
static std::random_device rd;
static std::mt19937 gen (rd ());
static std::uniform_int_distribution<> dis (0, 15);
static std::uniform_int_distribution<> dis2 (8, 11);

std::string generate_uuid_v4 () {
    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
	ss << dis (gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
	ss << dis (gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
	ss << dis (gen);
    }
    ss << "-";
    ss << dis2 (gen);
    for (i = 0; i < 3; i++) {
	ss << dis (gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
	ss << dis (gen);
    };
    return ss.str ();
}
}

WebBrowserContext::WebBrowserContext (WallpaperEngine::Application::WallpaperApplication& wallpaperApplication) :
    m_browserApplication (nullptr), m_wallpaperApplication (wallpaperApplication) {
    CefMainArgs main_args (
	this->m_wallpaperApplication.getContext ().getArgc (), this->m_wallpaperApplication.getContext ().getArgv ()
    );

    // only care about app if the process is the main process
    // we should maybe use a better lib for handling command line arguments instead
    // or using C's version on some places and CefCommandLine on others
    // TODO: ANOTHER THING TO TAKE CARE OF BEFORE MERGING
    const CefRefPtr<CefCommandLine> commandLine = CefCommandLine::CreateCommandLine ();

    commandLine->InitFromArgv (main_args.argc, main_args.argv);

    if (!commandLine->HasSwitch ("type")) {
	this->m_browserApplication = new CEF::BrowserApp (wallpaperApplication);
    } else {
	this->m_browserApplication = new CEF::SubprocessApp (wallpaperApplication);
    }

    // this blocks for anything not-main-thread
    const int exit_code = CefExecuteProcess (main_args, this->m_browserApplication, nullptr);

    // this is needed to kill subprocesses after they're done
    if (exit_code >= 0) {
	// Sub proccess has endend, so exit
	exit (exit_code);
    }

    // Configurate Chromium
    CefSettings settings;
    std::string cache_path = (std::filesystem::temp_directory_path () / uuid::generate_uuid_v4 ()).string ();
    // CefString(&settings.locales_dir_path) = "OffScreenCEF/godot/locales";
    // CefString(&settings.resources_dir_path) = "OffScreenCEF/godot/";
    // CefString(&settings.framework_dir_path) = "OffScreenCEF/godot/";
    // CefString(&settings.cache_path) = "OffScreenCEF/godot/";
    //  CefString(&settings.browser_subprocess_path) = "path/to/client"
    cef_string_utf8_to_utf16 (cache_path.c_str (), cache_path.length (), &settings.root_cache_path);
    settings.windowless_rendering_enabled = true;
#if defined(CEF_NO_SANDBOX)
    settings.no_sandbox = true;
#endif

    // spawns two new processess

    if (!CefInitialize (main_args, settings, this->m_browserApplication, nullptr)) {
	sLog.exception ("CefInitialize: failed");
    }
}

WebBrowserContext::~WebBrowserContext () {
    sLog.out ("Shutting down CEF");
    CefShutdown ();
}
