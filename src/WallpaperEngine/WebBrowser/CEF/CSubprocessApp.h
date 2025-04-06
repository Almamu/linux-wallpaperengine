#pragma once

#include "CWPSchemeHandlerFactory.h"
#include "WallpaperEngine/WebBrowser/CWebBrowserContext.h"
#include "include/cef_app.h"

namespace WallpaperEngine::Application {
class CWallpaperApplication;
}

namespace WallpaperEngine::WebBrowser::CEF {
class CSubprocessApp : public CefApp {
  public:
    explicit CSubprocessApp (WallpaperEngine::Application::CWallpaperApplication& application);

    void OnRegisterCustomSchemes (CefRawPtr <CefSchemeRegistrar> registrar) override;

  protected:
    const WallpaperEngine::Application::CWallpaperApplication& getApplication () const;
    const std::map<std::string, CWPSchemeHandlerFactory*>& getHandlerFactories () const;

  private:
    std::map<std::string, CWPSchemeHandlerFactory*> m_handlerFactories;
    WallpaperEngine::Application::CWallpaperApplication& m_application;
    IMPLEMENT_REFCOUNTING (CSubprocessApp);
    DISALLOW_COPY_AND_ASSIGN (CSubprocessApp);
};
}