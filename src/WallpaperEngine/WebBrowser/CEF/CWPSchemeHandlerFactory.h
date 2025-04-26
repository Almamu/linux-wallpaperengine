#pragma once

#include <string>
#include "WallpaperEngine/Core/CProject.h"
#include "include/cef_scheme.h"

namespace WallpaperEngine::WebBrowser::CEF {
/**
 * Simple factory that creates a scheme handler for wp when requested by Cef
 */
class CWPSchemeHandlerFactory : public CefSchemeHandlerFactory {
  public:
    explicit CWPSchemeHandlerFactory (const std::shared_ptr<const Core::CProject>& project);

    CefRefPtr<CefResourceHandler> Create (
        CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
        const CefString& scheme_name, CefRefPtr<CefRequest> request) override;

    static std::string generateSchemeName (const std::string& workshopId);
  private:
    std::shared_ptr<const Core::CProject> m_project;

    IMPLEMENT_REFCOUNTING (CWPSchemeHandlerFactory);
    DISALLOW_COPY_AND_ASSIGN (CWPSchemeHandlerFactory);
};
} // namespace WallpaperEngine::WebBrowser::CEF