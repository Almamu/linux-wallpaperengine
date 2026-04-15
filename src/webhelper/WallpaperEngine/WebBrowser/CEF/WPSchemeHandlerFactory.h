#pragma once

#include "include/cef_scheme.h"

namespace WallpaperEngine::WebBrowser::CEF {

class WPSchemeHandlerFactory : public CefSchemeHandlerFactory {
public:
    WPSchemeHandlerFactory () = default;

    CefRefPtr<CefResourceHandler> Create (
        CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
        const CefString& scheme_name, CefRefPtr<CefRequest> request
    ) override;

private:
    IMPLEMENT_REFCOUNTING (WPSchemeHandlerFactory);
    DISALLOW_COPY_AND_ASSIGN (WPSchemeHandlerFactory);
};

} // namespace WallpaperEngine::WebBrowser::CEF
