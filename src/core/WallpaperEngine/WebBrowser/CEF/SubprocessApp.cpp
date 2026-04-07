#include "SubprocessApp.h"
#include "WPSchemeHandlerFactory.h"
#include "WallpaperEngine/Data/Model/Project.h"

using namespace WallpaperEngine::WebBrowser::CEF;

SubprocessApp::SubprocessApp (const std::string& uuid, const AssetLocator& locator) :
	m_handlerFactory (locator), m_uuid (uuid), m_assetLoader (locator) { }

void SubprocessApp::OnRegisterCustomSchemes (CefRawPtr<CefSchemeRegistrar> registrar) {
	registrar->AddCustomScheme (
		WPSchemeHandlerFactory::generateSchemeName (this->getUUID ()),
		CEF_SCHEME_OPTION_STANDARD | CEF_SCHEME_OPTION_SECURE | CEF_SCHEME_OPTION_FETCH_ENABLED
	);
}

const std::string& SubprocessApp::getUUID () const { return this->m_uuid; }

WPSchemeHandlerFactory* SubprocessApp::getHandlerFactory () { return &this->m_handlerFactory; }