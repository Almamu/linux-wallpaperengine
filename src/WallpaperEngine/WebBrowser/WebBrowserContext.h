#pragma once

#include "WallpaperEngine/Application/ApplicationContext.h"
#include "WallpaperEngine/Application/WallpaperApplication.h"
#include "include/cef_app.h"
#include "include/cef_browser_process_handler.h"
#include "include/wrapper/cef_helpers.h"

#define WPENGINE_SCHEME "wp"

namespace WallpaperEngine::WebBrowser::CEF {
class BrowserApp;
}

namespace WallpaperEngine::WebBrowser {
    class WebBrowserContext {
      public:
        explicit WebBrowserContext (WallpaperEngine::Application::WallpaperApplication& wallpaperApplication);
        ~WebBrowserContext();

      private:
        CefRefPtr<CefApp> m_browserApplication = nullptr;
        CefRefPtr<CefCommandLine> m_commandLine = nullptr;
        WallpaperEngine::Application::WallpaperApplication& m_wallpaperApplication;
    };
} // namespace WallpaperEngine::WebBrowser
