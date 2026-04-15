#pragma once

#include "WPSchemeHandlerFactory.h"
#include "include/cef_app.h"

namespace WallpaperEngine::WebBrowser::CEF {
/**
 * Provides custom protocol to contain everything under it
 */
class BrowserApp : public CefApp, public CefBrowserProcessHandler {
public:
	BrowserApp ();

	[[nodiscard]] CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler () override;

	void OnContextInitialized () override;
	void OnRegisterCustomSchemes (CefRawPtr<CefSchemeRegistrar> registrar) override;
	void OnBeforeCommandLineProcessing (const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override;

private:
	CefRefPtr<WPSchemeHandlerFactory> m_handlerFactory;

	IMPLEMENT_REFCOUNTING (BrowserApp);
	DISALLOW_COPY_AND_ASSIGN (BrowserApp);
};
} // namespace WallpaperEngine::WebBrowser::CEF