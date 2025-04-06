#include "CWebBrowserContext.h"
#include "CEF/CBrowserApp.h"
#include "WallpaperEngine/Logging/CLog.h"
#include "WallpaperEngine/WebBrowser/CEF/CSubprocessApp.h"
#include "include/cef_app.h"
#include "include/cef_render_handler.h"
#include <filesystem>
#include <random>

using namespace WallpaperEngine::WebBrowser;

// TODO: THIS IS USED TO GENERATE A RANDOM FOLDER FOR THE CHROME PROFILE, MAYBE A DIFFERENT APPROACH WOULD BE BETTER?
namespace uuid {
static std::random_device              rd;
static std::mt19937                    gen(rd());
static std::uniform_int_distribution<> dis(0, 15);
static std::uniform_int_distribution<> dis2(8, 11);

std::string generate_uuid_v4() {
    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    };
    return ss.str();
}
}

CWebBrowserContext::CWebBrowserContext (WallpaperEngine::Application::CWallpaperApplication& wallpaperApplication) :
    m_wallpaperApplication (wallpaperApplication),
    m_browserApplication (nullptr) {
    CefMainArgs main_args (this->m_wallpaperApplication.getContext ().getArgc (), this->m_wallpaperApplication.getContext ().getArgv ());

    // only care about app if the process is the main process
    // we should maybe use a better lib for handling command line arguments instead
    // or using C's version on some places and CefCommandLine on others
    // TODO: ANOTHER THING TO TAKE CARE OF BEFORE MERGING
    CefRefPtr<CefCommandLine> commandLine = CefCommandLine::CreateCommandLine();

    commandLine->InitFromArgv (main_args.argc, main_args.argv);

    if (!commandLine->HasSwitch("type")) {
        this->m_browserApplication = new CEF::CBrowserApp(wallpaperApplication);
    } else {
        this->m_browserApplication = new CEF::CSubprocessApp(wallpaperApplication);
    }

    // this blocks for anything not-main-thread
    int exit_code = CefExecuteProcess (
        main_args, this->m_browserApplication, nullptr);

    // this is needed to kill subprocesses after they're done
    if (exit_code >= 0) {
        // Sub proccess has endend, so exit
        exit (exit_code);
    }

    // Configurate Chromium
    CefSettings settings;
    // CefString(&settings.locales_dir_path) = "OffScreenCEF/godot/locales";
    // CefString(&settings.resources_dir_path) = "OffScreenCEF/godot/";
    // CefString(&settings.framework_dir_path) = "OffScreenCEF/godot/";
    // CefString(&settings.cache_path) = "OffScreenCEF/godot/";
    //  CefString(&settings.browser_subprocess_path) = "path/to/client"
    CefString(&settings.root_cache_path) = std::filesystem::temp_directory_path() / uuid::generate_uuid_v4();
    settings.windowless_rendering_enabled = true;
#if defined(CEF_NO_SANDBOX)
    settings.no_sandbox = true;
#endif

    // spawns two new processess
    bool result = CefInitialize (main_args, settings, this->m_browserApplication, nullptr);

    if (!result) {
        sLog.exception ("CefInitialize: failed");
    }
}

CWebBrowserContext::~CWebBrowserContext () {
    sLog.out ("Shutting down CEF");
    CefShutdown ();
}
