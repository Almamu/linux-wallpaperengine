#pragma once

#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"
#include "include/wrapper/cef_helpers.h"

#define WPENGINE_SCHEME "wp"

namespace WallpaperEngine::WebBrowser::CEF {
class CBrowserApp;
}

namespace WallpaperEngine::WebBrowser {
    class CWebBrowserContext {
      public:
        explicit CWebBrowserContext (WallpaperEngine::Application::CWallpaperApplication& wallpaperApplication);
        ~CWebBrowserContext();

      private:
        CefRefPtr<CefApp> m_browserApplication = nullptr;
        CefRefPtr<CefCommandLine> m_commandLine = nullptr;
        WallpaperEngine::Application::CWallpaperApplication& m_wallpaperApplication;
    };
} // namespace WallpaperEngine::WebBrowser
