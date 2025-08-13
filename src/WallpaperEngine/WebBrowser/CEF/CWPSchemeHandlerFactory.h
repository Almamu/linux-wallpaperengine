#pragma once

#include <string>
#include "include/cef_scheme.h"

namespace WallpaperEngine::Data::Model {
class Project;
}

namespace WallpaperEngine::WebBrowser::CEF {
using namespace WallpaperEngine::Data::Model;

/**
 * Simple factory that creates a scheme handler for wp when requested by Cef
 */
class CWPSchemeHandlerFactory : public CefSchemeHandlerFactory {
  public:
    explicit CWPSchemeHandlerFactory (const Project& project);

    CefRefPtr<CefResourceHandler> Create (
        CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
        const CefString& scheme_name, CefRefPtr<CefRequest> request) override;

    static std::string generateSchemeName (const std::string& workshopId);
  private:
    const Project& m_project;

    IMPLEMENT_REFCOUNTING (CWPSchemeHandlerFactory);
    DISALLOW_COPY_AND_ASSIGN (CWPSchemeHandlerFactory);
};
} // namespace WallpaperEngine::WebBrowser::CEF