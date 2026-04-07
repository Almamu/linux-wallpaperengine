#pragma once

#include "WallpaperEngine/Assets/AssetLocator.h"
#include "include/cef_scheme.h"
#include <string>

namespace WallpaperEngine::Data::Model {
struct Project;
}

namespace WallpaperEngine::WebBrowser::CEF {
using namespace WallpaperEngine::Data::Model;

/**
 * Simple factory that creates a scheme handler for wp when requested by Cef
 */
class WPSchemeHandlerFactory : public CefSchemeHandlerFactory {
public:
	explicit WPSchemeHandlerFactory (const Assets::AssetLocator& assetLoader);

	CefRefPtr<CefResourceHandler> Create (
		CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, const CefString& scheme_name,
		CefRefPtr<CefRequest> request
	) override;

	static std::string generateSchemeName (const std::string& uuid);

private:
	const Assets::AssetLocator& m_assetLoader;

	IMPLEMENT_REFCOUNTING (WPSchemeHandlerFactory);
	DISALLOW_COPY_AND_ASSIGN (WPSchemeHandlerFactory);
};
} // namespace WallpaperEngine::WebBrowser::CEF