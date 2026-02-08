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
    // add back any parameters we had before so the new process can load up everything needed
    for (int i = 1; i < this->getApplication ().getContext ().getArgc (); i++) {
	command_line->AppendArgument (this->getApplication ().getContext ().getArgv ()[i]);
    }
}