#include "WPSchemeHandlerFactory.h"
#include "WPSchemeHandler.h"
#include "include/wrapper/cef_helpers.h"

#define WPENGINE_SCHEME "wp"

using namespace WallpaperEngine::WebBrowser::CEF;

WPSchemeHandlerFactory::WPSchemeHandlerFactory (const AssetLocator& assetLoader) : m_assetLoader (assetLoader) { }

CefRefPtr<CefResourceHandler> WPSchemeHandlerFactory::Create (
	CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& scheme_name,
	CefRefPtr<CefRequest> request
) {
	CEF_REQUIRE_IO_THREAD ();
	return new WPSchemeHandler (this->m_assetLoader);
}

std::string WPSchemeHandlerFactory::generateSchemeName (const std::string& uuid) {
	return std::string (WPENGINE_SCHEME) + uuid;
}