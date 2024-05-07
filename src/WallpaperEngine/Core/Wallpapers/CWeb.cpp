#include "CWeb.h"

#include "common.h"
#include <utility>

static void CEFsetUp (int argc, char** argv) {
    // This function should be called from the application entry point function to
    // execute a secondary process. It can be used to run secondary processes from
    // the browser client executable (default behavior) or from a separate
    // executable specified by the CefSettings.browser_subprocess_path value. If
    // called for the browser process (identified by no "type" command-line value)
    // it will return immediately with a value of -1. If called for a recognized
    // secondary process it will block until the process should exit and then return
    // the process exit code. The |application| parameter may be empty. The
    // |windows_sandbox_info| parameter is only used on Windows and may be NULL (see
    // cef_sandbox_win.h for details).
    CefMainArgs args (argc, argv);
    int exit_code = CefExecuteProcess (args, nullptr, nullptr);
    if (exit_code >= 0) {
        sLog.debug ("CEF sub proccess has endend");
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
    settings.windowless_rendering_enabled = true;
#if defined(CEF_NO_SANDBOX)
    settings.no_sandbox = true;
#endif

    bool result = CefInitialize (args, settings, nullptr, nullptr);
    if (!result) {
        sLog.error ("CefInitialize: failed");
        exit (-2);
    }
}

using namespace WallpaperEngine::Core;

const std::string& CWeb::getFilename () {
    return this->m_filename;
}

CWeb::CWeb (std::string filename, CProject& project) : CWallpaper (Type, project), m_filename (std::move (filename)) {
    if (!g_CEFused) {
        sLog.debug ("Setting up CEF");
        // char** argv = new char*("linux-wallpaper\n");
        // CEFsetUp(1, argv);
        // delete argv;
        g_CEFused = true;
    }
}

const std::string CWeb::Type = "web";
