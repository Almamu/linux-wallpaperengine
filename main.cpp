#include <csignal>
#include <iostream>

#include "WallpaperEngine/Application/CApplicationContext.h"
#include "WallpaperEngine/Application/CWallpaperApplication.h"
#include "WallpaperEngine/Core/CWeb.h"
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

static void CEFsetUp(int argc, char** argv)
{
    // This function should be called from the application entry point function to
    // execute a secondary process. It can be used to run secondary processes from
    // the browser client executable (default behavior) or from a separate
    // executable specified by the CefSettings.browser_subprocess_path value. If
    // called for the browser process (identified by no "type" command-line value)
    // it will return immediately with a value of -1. If called for a recognized
    // secondary process it will block until the process should exit and then return
    // the process exit code. The |application| parameter may be empty. The
    // |windows_sandbox_info| parameter is only used on Windows and may be NULL (see
    // cef_sandbox_win.h for details).
    
    CefMainArgs args(argc, argv);
    
    int exit_code = CefExecuteProcess(args, nullptr, nullptr);//Spawned processes will terminate here(see CefIninitilize below). Maybe implementing settings.browser_subprocess_path will allow it to work not in main function.
    if (exit_code >= 0)
    {
        // Sub proccess has endend, so exit
        exit(exit_code);
    }
    else if (exit_code == -1)
    {
        // If called for the browser process (identified by no "type" command-line value)
        // it will return immediately with a value of -1
    }

    // Configurate Chromium
    CefSettings settings;
    //CefString(&settings.locales_dir_path) = "OffScreenCEF/godot/locales";
    //CefString(&settings.resources_dir_path) = "OffScreenCEF/godot/";
    //CefString(&settings.framework_dir_path) = "OffScreenCEF/godot/";
    //CefString(&settings.cache_path) = "OffScreenCEF/godot/";
    // CefString(&settings.browser_subprocess_path) = "path/to/client"
    settings.windowless_rendering_enabled = true;
#if defined(CEF_NO_SANDBOX)
    settings.no_sandbox = true;
#endif

    bool result = CefInitialize(args, settings, nullptr, nullptr); //Spawn 2 new processes; Can be moved to Core::CWeb
    if (!result)
    {
        std::cerr << "CefInitialize: failed" << std::endl;
        exit(-2);
    }

}

bool g_CEFused=false;//Will be set to true if wallpaper has "web" type 
int main (int argc, char* argv[])
{
    //START of CEF init block(it will run 3 times)
    char** argv2 = new char*[argc]; //Cef modify argv on CefInit, copy it before that

    for(int i=0; i<argc; ++i)
    {
        argv2[i] = new char[strlen(argv[i])+1];
        strcpy(argv2[i],argv[i]);
    }
    CEFsetUp(argc,argv);//Cef will launch new process with main(argc,argv) twice. If we won't pass argc and argv from main, we will create fork bomb and system will freeze until reboot. 
    //END of CEF init block 
    initLogging ();
    WallpaperEngine::Application::CApplicationContext appContext (argc, argv2);
    WallpaperEngine::Application::CWallpaperApplication app (appContext);

    // halt if the list-properties option was specified
    if (appContext.settings.general.onlyListProperties)
        return 0;

    appPointer = &app;

    // attach signals to gracefully stop
    std::signal (SIGINT, signalhandler);
    std::signal (SIGTERM, signalhandler);

    if(!g_CEFused){
        sLog.debug("No web wallpapers, shutting down CEF");
        CefShutdown();
    }
    // show the wallpaper application
    app.show ();

    if(g_CEFused){
        sLog.debug("Shutting down CEF");
        CefShutdown();
    }

    appPointer = nullptr;
    return 0;
}