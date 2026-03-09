#include "BrowserApp.h"

#include <utility>
#include "WallpaperEngine/Utils/UUID.h"

using namespace WallpaperEngine::WebBrowser::CEF;

BrowserApp::BrowserApp (
	std::filesystem::path  assetDir, std::filesystem::path  backgroundDir,
	const Assets::AssetLocator& locator
) : SubprocessApp (Utils::UUID::UUIDv4 (), locator), m_assetDir (std::move(assetDir)), m_backgroundDir (std::move(backgroundDir)) { }

CefRefPtr<CefBrowserProcessHandler> BrowserApp::GetBrowserProcessHandler () { return this; }

void BrowserApp::OnContextInitialized () {
	CefRegisterSchemeHandlerFactory (
		WPSchemeHandlerFactory::generateSchemeName (this->getUUID ()), static_cast<const char*> (nullptr),
		this->getHandlerFactory ()
	);
}

void BrowserApp::OnBeforeCommandLineProcessing (const CefString& process_type, CefRefPtr<CefCommandLine> command_line) {
	command_line->AppendSwitchWithValue (
		"--disable-features",
		"IsolateOrigins,HardwareMediaKeyHandling,WebContentsOcclusion,RendererCodeIntegrityEnabled,site-per-process"
	);
	command_line->AppendSwitch ("--disable-gpu-shader-disk-cache");
	command_line->AppendSwitch ("--disable-site-isolation-trials");
	command_line->AppendSwitch ("--disable-web-security");
	command_line->AppendSwitchWithValue ("--remote-allow-origins", "*");
	command_line->AppendSwitchWithValue ("--autoplay-policy", "no-user-gesture-required");
	command_line->AppendSwitch ("--disable-background-timer-throttling");
	command_line->AppendSwitch ("--disable-backgrounding-occluded-windows");
	command_line->AppendSwitch ("--disable-background-media-suspend");
	command_line->AppendSwitch ("--disable-renderer-backgrounding");
	command_line->AppendSwitch ("--disable-test-root-certs");
	command_line->AppendSwitch ("--disable-bundled-ppapi-flash");
	command_line->AppendSwitch ("--disable-breakpad");
	command_line->AppendSwitch ("--disable-field-trial-config");
	command_line->AppendSwitch ("--no-experiments");
	// TODO: ACTIVATE THIS IF WE EVER SUPPORT MACOS OFFICIALLY
	/*
if (process_type.empty()) {
#if defined(OS_MACOSX)
  // Disable the macOS keychain prompt. Cookies will not be encrypted.
  command_line->AppendSwitch("use-mock-keychain");
#endif
}*/
}

void BrowserApp::OnBeforeChildProcessLaunch (CefRefPtr<CefCommandLine> command_line) {
	// TODO: add some parameters to give more context on what to load
	command_line->AppendSwitchWithValue ("uuid", this->getUUID ());
	command_line->AppendSwitchWithValue ("assets-dir", this->m_assetDir.c_str ());
	command_line->AppendSwitchWithValue ("background-dir", this->m_backgroundDir.c_str ());
}