#include "WebBrowserContext.h"
#include "CEF/BrowserApp.h"
#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/WebBrowser/CEF/SubprocessApp.h"
#include "include/cef_app.h"
#include "include/cef_render_handler.h"
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <unistd.h>

using namespace WallpaperEngine::WebBrowser;

namespace {
/**
 * Per-run Chromium profile folder: $XDG_RUNTIME_DIR/linux-wallpaperengine/cef-<pid>
 * (falls back to the system temp dir). It is removed again in ~WebBrowserContext; folders
 * left behind by runs that crashed are cleaned up the next time a browser starts.
 */
std::filesystem::path profileRoot () {
    const char* runtime = std::getenv ("XDG_RUNTIME_DIR");
    std::filesystem::path base = (runtime != nullptr && *runtime != '\0' && std::filesystem::is_directory (runtime))
				     ? std::filesystem::path (runtime)
				     : std::filesystem::temp_directory_path ();
    return base / "linux-wallpaperengine";
}

void removeStaleProfiles (const std::filesystem::path& root) {
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator (root, ec)) {
	const std::string name = entry.path ().filename ().string ();
	if (!entry.is_directory () || name.rfind ("cef-", 0) != 0) {
	    continue;
	}
	const std::string pid = name.substr (4);
	if (pid.empty () || pid.find_first_not_of ("0123456789") != std::string::npos) {
	    continue;
	}
	// still owned by a running engine
	if (std::filesystem::exists ("/proc/" + pid)) {
	    continue;
	}
	std::filesystem::remove_all (entry.path (), ec);
    }
}
} // namespace

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
    const std::filesystem::path root = profileRoot ();
    std::error_code ec;
    std::filesystem::create_directories (root, ec);
    std::filesystem::permissions (root, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace, ec);
    removeStaleProfiles (root);
    this->m_cachePath = root / ("cef-" + std::to_string (getpid ()));
    const std::string cache_path = this->m_cachePath.string ();
    // CefString(&settings.locales_dir_path) = "OffScreenCEF/godot/locales";
    // CefString(&settings.resources_dir_path) = "OffScreenCEF/godot/";
    // CefString(&settings.framework_dir_path) = "OffScreenCEF/godot/";
    // CefString(&settings.cache_path) = "OffScreenCEF/godot/";
    //  CefString(&settings.browser_subprocess_path) = "path/to/client"
    cef_string_utf8_to_utf16 (cache_path.c_str (), cache_path.length (), &settings.root_cache_path);
    settings.windowless_rendering_enabled = true;
    // Gated by CMake option CEF_NO_SANDBOX (default ON). Local/AUR builds rarely
    // have chrome-sandbox setuid-root; without no_sandbox, CefInitialize often
    // dies before the first web wallpaper paint. Packagers with a working
    // setuid helper can configure -DCEF_NO_SANDBOX=OFF.
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

    if (!this->m_cachePath.empty ()) {
	std::error_code ec;
	std::filesystem::remove_all (this->m_cachePath, ec);
    }
}
