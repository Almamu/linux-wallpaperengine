#include "WallpaperEngine/Context.h"
#include "WallpaperEngine/WebBrowser/CEF/SubprocessApp.h"
#include "include/cef_app.h"

int main (int argc, char* argv[]) {
	CefMainArgs main_args (argc, argv);

	// get paths off the command-line arguments
	const CefRefPtr<CefCommandLine> commandLine = CefCommandLine::CreateCommandLine ();

	commandLine->InitFromArgv (main_args.argc, main_args.argv);

	const CefString assetsDir = commandLine->GetSwitchValue ("assets-dir");
	const CefString backgroundDir = commandLine->GetSwitchValue ("background-dir");
	const CefString backgroundId = commandLine->GetSwitchValue ("uuid");

	// configure the asset container so the subprocess can access the assets
	auto container = std::make_unique<WallpaperEngine::FileSystem::Container> ();

	// web backgrounds only need the project's path and asset's path
	container->mount (backgroundDir.c_str (), "/");
	container->mount (assetsDir.c_str (), "/assets");

	auto locator = std::make_unique<WallpaperEngine::Assets::AssetLocator> (std::move (container));

	WallpaperEngine::WebBrowser::CEF::SubprocessApp subprocess (backgroundId, *locator);

	return CefExecuteProcess (main_args, &subprocess, nullptr);
}