#include <csignal>
#include <iostream>

#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "WallpaperEngine/WebBrowser/CWebBrowserContext.h"
#include "common.h"

WallpaperEngine::Application::CWallpaperApplication* app;

void signalhandler(int sig)
{
    if (app == nullptr)
        return;

    app->signal (sig);
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

    app = new WallpaperEngine::Application::CWallpaperApplication (appContext, webBrowserContext);

    // attach signals to gracefully stop
    std::signal (SIGINT, signalhandler);
    std::signal (SIGTERM, signalhandler);

    // show the wallpaper application
    app->show ();

    // remove signal handlers before destroying app
    std::signal (SIGINT, SIG_DFL);
    std::signal (SIGTERM, SIG_DFL);

    delete app;

    return 0;
}