#include <FreeImage.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <SDL.h>
#include <csignal>
#include <filesystem>
#include <getopt.h>
#include <iostream>
#include <libgen.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Render/CRenderContext.h"
#include "WallpaperEngine/Render/CVideo.h"
#include "WallpaperEngine/Render/CWallpaper.h"

#include "WallpaperEngine/Assets/CPackage.h"
#include "WallpaperEngine/Assets/CDirectory.h"
#include "WallpaperEngine/Assets/CVirtualContainer.h"
#include "WallpaperEngine/Assets/CCombinedContainer.h"
#include "WallpaperEngine/Assets/CPackageLoadException.h"

#include "Steam/FileSystem/FileSystem.h"
#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "WallpaperEngine/Audio/CAudioContext.h"
#include "WallpaperEngine/Audio/Drivers/CSDLAudioDriver.h"
#include "WallpaperEngine/Render/Drivers/COpenGLDriver.h"
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

int main (int argc, char* argv[])
{
    initLogging ();

    WallpaperEngine::Application::CApplicationContext appContext (argc, argv);
    WallpaperEngine::Application::CWallpaperApplication app (appContext);

    // halt if the list-properties option was specified
    if (appContext.onlyListProperties)
        return 0;

    appPointer = &app;

    // attach signals to gracefully stop
    std::signal (SIGINT, signalhandler);
    std::signal (SIGTERM, signalhandler);

    // show the wallpaper application
    app.show ();

    appPointer = nullptr;

    return 0;
}