#pragma once

#include "SubprocessApp.h"
#include "WPSchemeHandlerFactory.h"
#include "include/cef_app.h"

namespace WallpaperEngine::Application {
class WallpaperApplication;
}

namespace WallpaperEngine::WebBrowser::CEF {
/**
 * Provides custom protocol to contain everything under it
 */
class BrowserApp : public SubprocessApp, public CefBrowserProcessHandler {
public:
	explicit BrowserApp (
		std::filesystem::path assetDir, std::filesystem::path backgroundDir, const Assets::AssetLocator& locator
	);

	[[nodiscard]] CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler () override;

	void OnContextInitialized () override;
	void OnBeforeCommandLineProcessing (const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override;
	void OnBeforeChildProcessLaunch (CefRefPtr<CefCommandLine> command_line) override;

private:
	std::filesystem::path m_assetDir;
	std::filesystem::path m_backgroundDir;

	IMPLEMENT_REFCOUNTING (BrowserApp);
	DISALLOW_COPY_AND_ASSIGN (BrowserApp);
};
} // namespace WallpaperEngine::WebBrowser::CEF