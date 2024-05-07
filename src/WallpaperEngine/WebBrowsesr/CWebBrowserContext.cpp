#include "CWebBrowserContext.h"
#include "WallpaperEngine/Logging/CLog.h"
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_render_handler.h"

using namespace WallpaperEngine::WebBrowser;

CWebBrowserContext::CWebBrowserContext (int argc, char** argv) : m_stopped (false) {
    // clone original argc/argv as they'll be modified by cef
    char** argv2 = new char*[argc];

    for (int i = 0; i < argc; i++) {
        argv2 [i] = new char [strlen (argv [i]) + 1];
        strcpy (argv2 [i], argv [i]);
    }

    CefMainArgs args (argc, argv2);

    int exit_code = CefExecuteProcess (
        args, nullptr, nullptr); // Spawned processes will terminate here(see CefIninitilize below). Maybe implementing
                                 // settings.browser_subprocess_path will allow it to work not in main function.
    if (exit_code >= 0) {
        // Sub proccess has endend, so exit
        exit (exit_code);
    } else if (exit_code == -1) {
        // If called for the browser process (identified by no "type" command-line value)
        // it will return immediately with a value of -1
    }

    // Configurate Chromium
    CefSettings settings;
    // CefString(&settings.locales_dir_path) = "OffScreenCEF/godot/locales";
    // CefString(&settings.resources_dir_path) = "OffScreenCEF/godot/";
    // CefString(&settings.framework_dir_path) = "OffScreenCEF/godot/";
    // CefString(&settings.cache_path) = "OffScreenCEF/godot/";
    //  CefString(&settings.browser_subprocess_path) = "path/to/client"
    settings.windowless_rendering_enabled = true;
#if defined(CEF_NO_SANDBOX)
    settings.no_sandbox = true;
#endif

    // spawns two new processess
    bool result = CefInitialize (args, settings, nullptr, nullptr);

    if (!result) {
        sLog.exception ("CefInitialize: failed");
    }
}

CWebBrowserContext::~CWebBrowserContext () {
    this->stop ();
}

void CWebBrowserContext::markAsUsed () {
    this->m_inUse = true;
}

bool CWebBrowserContext::isUsed () {
    return this->m_inUse;
}

void CWebBrowserContext::stop () {
    if (this->m_stopped) {
        return;
    }

    sLog.out ("Shutting down CEF");

    this->m_stopped = true;

    CefShutdown ();
}