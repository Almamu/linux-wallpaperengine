#include "BrowserApp.h"
#include "WallpaperEngine/Logging/Log.h"

using namespace WallpaperEngine::WebBrowser::CEF;

BrowserApp::BrowserApp (WallpaperEngine::Application::WallpaperApplication& application) :
    SubprocessApp (application) { }

CefRefPtr<CefBrowserProcessHandler> BrowserApp::GetBrowserProcessHandler () { return this; }

void BrowserApp::OnContextInitialized () {
    // register all the needed schemes, "wp" + the background id is going to be our scheme
    for (const auto& [workshopId, factory] : this->getHandlerFactories ()) {
	CefRegisterSchemeHandlerFactory (
	    WPSchemeHandlerFactory::generateSchemeName (workshopId), static_cast<const char*> (nullptr), factory
	);
    }
}

void BrowserApp::OnBeforeCommandLineProcessing (const CefString& process_type, CefRefPtr<CefCommandLine> command_line) {
    command_line->AppendSwitchWithValue (
	"--disable-features",
	"IsolateOrigins,HardwareMediaKeyHandling,WebContentsOcclusion,RendererCodeIntegrityEnabled,site-per-process"
    );
    // Pair with settings.no_sandbox in WebBrowserContext — required when
    // chrome-sandbox is not setuid root (typical non-packaged local build).
    command_line->AppendSwitch ("--no-sandbox");
    command_line->AppendSwitch ("--disable-gpu-sandbox");
    // Avoid a separate GPU process (often fails on Wayland/Hyprland with CEF
    // windowless rendering — error_code=1002 "GPU process isn't usable").
    command_line->AppendSwitch ("--in-process-gpu");
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
    // Do NOT re-append the full wallpaper-engine argv. That used to force every
    // CEF child to re-enter ApplicationContext/WallpaperApplication (and often
    // crash). Instead pass only the custom scheme IDs so EarlyCefSubprocessApp
    // in main.cpp can register matching wp{id} schemes.
    std::string schemeIds;
    for (const auto& workshopId : this->getHandlerFactories () | std::views::keys) {
	if (!schemeIds.empty ()) {
	    schemeIds.push_back (',');
	}
	schemeIds += workshopId;
    }
    if (!schemeIds.empty ()) {
	command_line->AppendSwitchWithValue ("wp-schemes", schemeIds);
    }
}