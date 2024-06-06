#include <csignal>
#include <iostream>

#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "WallpaperEngine/WebBrowser/CWebBrowserContext.h"
#include "common.h"

WallpaperEngine::Application::CWallpaperApplication* appPointer;

void signalhandler(int sig)
{
    if (appPointer == nullptr)
        return;

    appPointer->signal (sig);
}

void initLogging ()
{
    sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
    sLog.addError (new std::ostream (std::cerr.rdbuf ()));
}

int main (int argc, char* argv[]) {
    initLogging ();
    WallpaperEngine::WebBrowser::CWebBrowserContext webBrowserContext(argc, argv);
    WallpaperEngine::Application::CApplicationContext appContext (argc, argv);

    // halt if the list-properties option was specified
    if (appContext.settings.general.onlyListProperties)
        return 0;

    WallpaperEngine::Application::CWallpaperApplication app (appContext, webBrowserContext);

    appPointer = &app;

    // attach signals to gracefully stop
    std::signal (SIGINT, signalhandler);
    std::signal (SIGTERM, signalhandler);

    // show the wallpaper application
    app.show ();

    appPointer = nullptr;
    return 0;
}