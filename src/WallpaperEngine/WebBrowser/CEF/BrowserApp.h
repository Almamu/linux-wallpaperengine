#pragma once

#include "SubprocessApp.h"
#include "WPSchemeHandlerFactory.h"
#include "WallpaperEngine/WebBrowser/WebBrowserContext.h"
#include "include/cef_app.h"

namespace WallpaperEngine::Application {
class WallpaperApplication;
}

namespace WallpaperEngine::WebBrowser::CEF {
/**
 * Provides custom protocol to contain everything under it
 */
class BrowserApp : public SubprocessApp, public CefBrowserProcessHandler {
public:
    explicit BrowserApp (WallpaperEngine::Application::WallpaperApplication& application);

    [[nodiscard]] CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler () override;

    void OnContextInitialized () override;
    void OnBeforeCommandLineProcessing (const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override;
    void OnBeforeChildProcessLaunch (CefRefPtr<CefCommandLine> command_line) override;

private:
    IMPLEMENT_REFCOUNTING (BrowserApp);
    DISALLOW_COPY_AND_ASSIGN (BrowserApp);
};
} // namespace WallpaperEngine::WebBrowser::CEF