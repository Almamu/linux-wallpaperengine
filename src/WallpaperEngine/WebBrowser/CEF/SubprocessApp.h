#pragma once

#include "WPSchemeHandlerFactory.h"
#include "include/cef_app.h"

namespace WallpaperEngine::WebBrowser::CEF {
class SubprocessApp : public CefApp {
public:
	explicit SubprocessApp (const std::string& uuid, const Assets::AssetLocator& locator);

	void OnRegisterCustomSchemes (CefRawPtr<CefSchemeRegistrar> registrar) override;

protected:
	const std::string& getUUID () const;
	WPSchemeHandlerFactory* getHandlerFactory ();

private:
	WPSchemeHandlerFactory m_handlerFactory;
	std::string m_uuid;
	const Assets::AssetLocator& m_assetLoader;
	IMPLEMENT_REFCOUNTING (SubprocessApp);
	DISALLOW_COPY_AND_ASSIGN (SubprocessApp);
};
}