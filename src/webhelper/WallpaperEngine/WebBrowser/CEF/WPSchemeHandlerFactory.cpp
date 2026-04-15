#include "WPSchemeHandlerFactory.h"

#include "include/cef_parser.h"
#include "include/wrapper/cef_helpers.h"
#include "WallpaperEngine/WebBrowser/IPC/BrowserServer.h"
#include "WPSchemeHandler.h"

using namespace WallpaperEngine::WebBrowser::CEF;

CefRefPtr<CefResourceHandler> WPSchemeHandlerFactory::Create (
    CefRefPtr<CefBrowser> /*browser*/, CefRefPtr<CefFrame> /*frame*/,
    const CefString& /*scheme_name*/, CefRefPtr<CefRequest> request
) {
    CEF_REQUIRE_IO_THREAD ();

    CefURLParts url_parts;
    if (!CefParseURL (request->GetURL (), url_parts))
        return nullptr;

    const std::string uuid = CefString (&url_parts.host).ToString ();
    return new WPSchemeHandler (uuid, &sBrowserServer);
}
