#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

#include <fcntl.h>
#include <unistd.h>

#include "include/cef_app.h"

#include "WallpaperEngine/Logging/Log.h"
#include "WallpaperEngine/Utils/UUID.h"
#include "WallpaperEngine/WebBrowser/CEF/BrowserApp.h"
#include "WallpaperEngine/WebBrowser/IPC/BrowserServer.h"

#include <filesystem>

// Minimal CefApp used only in renderer/GPU/utility subprocess re-invocations.
// It registers the wp:// custom scheme but intentionally has NO browser-process
// handler — returning one here causes GPU/renderer subprocesses to invoke
// browser-only callbacks and hang inside CefExecuteProcess.
class SubprocessApp : public CefApp {
public:
    SubprocessApp () = default;

    void OnRegisterCustomSchemes (CefRawPtr<CefSchemeRegistrar> registrar) override {
	registrar->AddCustomScheme (
	    "wp", CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE | CEF_SCHEME_OPTION_FETCH_ENABLED
	);
    }
    IMPLEMENT_REFCOUNTING (SubprocessApp);
    DISALLOW_COPY_AND_ASSIGN (SubprocessApp);
};

int main (int argc, char* argv[]) {
    sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
    sLog.addError (new std::ostream (std::cerr.rdbuf ()));

    // Let CEF handle renderer / GPU / utility subprocess re-invocations first.
    // Use a minimal app here — the full BrowserApp exposes GetBrowserProcessHandler()
    // which causes CEF to invoke browser-only callbacks in subprocess contexts and hang.
    CefMainArgs main_args (argc, argv);
    const int exitCode = CefExecuteProcess (main_args, new SubprocessApp (), nullptr);
    if (exitCode >= 0) {
	return exitCode;
    }

    // Browser process — parse --ipc-fd=N.
    int ipcFd = -1;
    for (int i = 1; i < argc; ++i) {
	const std::string_view arg (argv[i]);
	if (arg.starts_with ("--ipc-fd=")) {
	    ipcFd = std::atoi (argv[i] + 9);
	    break;
	}
    }

    if (ipcFd < 0) {
	sLog.error ("WebBrowserHelper: missing --ipc-fd argument");
	return 1;
    }

    // Prevent CEF renderer sub-processes from inheriting the socket.
    fcntl (ipcFd, F_SETFD, fcntl (ipcFd, F_GETFD) | FD_CLOEXEC);

    CefSettings settings;
    settings.windowless_rendering_enabled = true;
    settings.background_color = 0xFF000000;

#ifdef WPENGINE_CEF_RESOURCES_PATH
    CefString (&settings.resources_dir_path).FromString (WPENGINE_CEF_RESOURCES_PATH);
    CefString (&settings.locales_dir_path).FromString (WPENGINE_CEF_RESOURCES_PATH "/locales");
#endif

    // use a random root cache path as to not block
    CefString (&settings.root_cache_path)
	.FromString (std::filesystem::temp_directory_path () / WallpaperEngine::Utils::UUID::UUIDv4 ());

#ifdef CEF_NO_SANDBOX
    settings.no_sandbox = true;
#endif

    sLog.out ("WebBrowserHelper: calling CefInitialize");
    auto* app = new WallpaperEngine::WebBrowser::CEF::BrowserApp ();
    if (!CefInitialize (main_args, settings, app, nullptr)) {
	sLog.error ("WebBrowserHelper: CefInitialize failed (exit code ", CefGetExitCode (), ")");
	return 1;
    }
    sLog.out ("WebBrowserHelper: CefInitialize succeeded");

    // Start IPC after CefInitialize so that CefPostTask to TID_UI is safe
    // to call from the receive thread.
    WallpaperEngine::WebBrowser::IPC::BrowserServer::init (ipcFd);
    sLog.out ("WebBrowserHelper: BrowserServer initialized, entering message loop");

    CefRunMessageLoop ();
    sLog.out ("WebBrowserHelper: message loop exited");

    WallpaperEngine::WebBrowser::IPC::BrowserServer::shutdown ();
    CefShutdown ();
    return 0;
}
