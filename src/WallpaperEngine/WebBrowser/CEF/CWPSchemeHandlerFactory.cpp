#include "CWPSchemeHandlerFactory.h"
#include "CWPSchemeHandler.h"
#include "WallpaperEngine/WebBrowser/CWebBrowserContext.h"
#include "include/wrapper/cef_helpers.h"

using namespace WallpaperEngine::WebBrowser::CEF;

CWPSchemeHandlerFactory::CWPSchemeHandlerFactory (const Project& project) :
    m_project (project) {}

CefRefPtr<CefResourceHandler> CWPSchemeHandlerFactory::Create (
    CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
    const CefString& scheme_name, CefRefPtr<CefRequest> request) {
    CEF_REQUIRE_IO_THREAD ();
    return new CWPSchemeHandler(this->m_project);
}

std::string CWPSchemeHandlerFactory::generateSchemeName (const std::string& workshopId) {
    return std::string(WPENGINE_SCHEME) + workshopId;
}