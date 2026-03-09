#include "WallpaperEngine/Context.h"
#include "WallpaperEngine/WebBrowser/CEF/SubprocessApp.h"
#include "include/cef_app.h"

int main (int argc, char* argv[]) {
	CefMainArgs main_args (argc, argv);

	sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
	sLog.addError (new std::ostream (std::cerr.rdbuf ()));

	// get paths off the command-line arguments
	const CefRefPtr<CefCommandLine> commandLine = CefCommandLine::CreateCommandLine ();

	commandLine->InitFromArgv (main_args.argc, main_args.argv);

	const CefString assetsDir = commandLine->GetSwitchValue ("assets-dir");
	const CefString backgroundDir = commandLine->GetSwitchValue ("background-dir");
	const CefString backgroundId = commandLine->GetSwitchValue ("uuid");

	if (assetsDir.empty() || backgroundDir.empty() || backgroundId.empty()) {
		sLog.error ("Missing required command-line arguments. This command is designed to be run from liblinux-wallpaperengine-core and not by itself...");
		return 1;
	}

	// configure the asset container so the subprocess can access the assets
	auto container = std::make_unique<WallpaperEngine::FileSystem::Container> ();

	// web backgrounds only need the project's path and asset's path
	container->mount (backgroundDir.c_str (), "/");
	container->mount (assetsDir.c_str (), "/assets");

	auto locator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (container));

	WallpaperEngine::WebBrowser::CEF::SubprocessApp subprocess (backgroundId, *locator);

	return CefExecuteProcess (main_args, &subprocess, nullptr);
}