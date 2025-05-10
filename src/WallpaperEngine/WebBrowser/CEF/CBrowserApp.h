#pragma once

#include "CSubprocessApp.h"
#include "CWPSchemeHandlerFactory.h"
#include "WallpaperEngine/WebBrowser/CWebBrowserContext.h"
#include "include/cef_app.h"

namespace WallpaperEngine::Application {
class CWallpaperApplication;
}

namespace WallpaperEngine::WebBrowser::CEF {
/**
 * Provides custom protocol to contain everything under it
 */
class CBrowserApp : public CSubprocessApp, public CefBrowserProcessHandler {
  public:
    explicit CBrowserApp (WallpaperEngine::Application::CWallpaperApplication& application);

    [[nodiscard]] CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler () override;

    void OnContextInitialized () override;
    void OnBeforeCommandLineProcessing (const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override;
    void OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> command_line) override;

  private:
    IMPLEMENT_REFCOUNTING (CBrowserApp);
    DISALLOW_COPY_AND_ASSIGN (CBrowserApp);
};
} // namespace WallpaperEngine::WebBrowser::CEF