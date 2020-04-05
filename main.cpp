#include <iostream>
#include <irrlicht/irrlicht.h>
#include <getopt.h>
#include <SDL_mixer.h>
#include <SDL.h>

#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Irrlicht/CContext.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/CScene.h"
#include "WallpaperEngine/Render/CVideo.h"

enum BACKGROUND_RUN_MODE
{
    RUN_MODE_UNKNOWN = 0,
    RUN_MODE_HELP = 1,
    RUN_MODE_DIRECTORY = 2,
    RUN_MODE_PACKAGE = 3
};

WallpaperEngine::Irrlicht::CContext* IrrlichtContext = nullptr;

void print_help (const char* route)
{
    std::cout
        << "Usage:" << route << " [options] " << std::endl
        << "options:" << std::endl
        << "  --silent\t\tMutes all the sound the wallpaper might produce" << std::endl
        << "  --dir <folder>\tLoads an uncompressed background from the given <folder>" << std::endl
        << "  --pkg <folder>\tLoads a scene.pkg file from the given <folder>" << std::endl
        << "  --screen-root <screen name>\tDisplay as screen's background" << std::endl
        << "  --fps <maximum-fps>\tLimits the FPS to the given number, useful to keep battery consumption low" << std::endl;
}

std::string stringPathFixes(const std::string& s){
    std::string str(s);
    if(str.empty())
        return s;
    if(str[0] == '\'' && str[str.size() - 1] == '\''){
        str.erase(str.size() - 1, 1);
        str.erase(0,1);
    }
    if(str[str.size() - 1] != '/')
        str += '/';
    return std::move(str);
}

int main (int argc, char* argv[])
{
    std::vector <std::string> screens;
    bool isRootWindow = false;

    int mode = RUN_MODE_UNKNOWN;
    int maximumFPS = 30;
    bool shouldEnableAudio = true;
    std::string path;

    int option_index = 0;

    static struct option long_options [] = {
        {"screen-root", required_argument, 0, 'r'},
        {"pkg",         required_argument, 0, 'p'},
        {"dir",         required_argument, 0, 'd'},
        {"silent",      no_argument,       0, 's'},
        {"help",        no_argument,       0, 'h'},
        {"fps",         required_argument, 0, 'f'},
        {nullptr,              0, 0,   0}
    };

    while (true)
    {
        int c = getopt_long (argc, argv, "r:p:d:shf:", long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 'r':
                isRootWindow = true;
                screens.emplace_back (optarg);
                break;

            case 'p':
                if (mode == RUN_MODE_UNKNOWN)
                    mode = RUN_MODE_PACKAGE;
                path = optarg;
                break;

            case 'd':
                if (mode == RUN_MODE_UNKNOWN)
                    mode = RUN_MODE_DIRECTORY;
                path = optarg;
                break;

            case 's':
                shouldEnableAudio = false;
                break;

            case 'h':
                mode = RUN_MODE_HELP;
                break;

            case 'f':
                maximumFPS = atoi (optarg);
                break;

            default:
                break;
        }
    }

    if (mode == RUN_MODE_UNKNOWN || mode == RUN_MODE_HELP)
    {
        print_help (argv [0]);
        return 0;
    }

    try
    {
        IrrlichtContext = new WallpaperEngine::Irrlicht::CContext (screens, isRootWindow);
        IrrlichtContext->initializeContext ();
    }
    catch (std::runtime_error& ex)
    {
        std::cerr << ex.what () << std::endl;

        return 1;
    }

    path = stringPathFixes (path);

    irr::io::path wallpaper_path = IrrlichtContext->getDevice ()->getFileSystem ()->getAbsolutePath (path.c_str ());
    irr::io::path project_path = wallpaper_path + "project.json";

    if (mode == RUN_MODE_PACKAGE)
    {
        irr::io::path scene_path = wallpaper_path + "scene.pkg";
        // add the package file to the lookup list
        IrrlichtContext->getDevice ()->getFileSystem ()->addFileArchive (scene_path, true, false);
    }
    else if (mode == RUN_MODE_DIRECTORY)
    {
        project_path = wallpaper_path + "project.json";
        // set the working directory to the project folder
        IrrlichtContext->getDevice ()->getFileSystem ()->changeWorkingDirectoryTo (wallpaper_path);
    }

    if (shouldEnableAudio == true)
    {
        int mixer_flags = MIX_INIT_MP3 | MIX_INIT_FLAC | MIX_INIT_OGG;

        if (SDL_Init (SDL_INIT_AUDIO) < 0 || mixer_flags != Mix_Init (mixer_flags))
        {
            // Mix_GetError is an alias for SDL_GetError, so calling it directly will yield the correct result
            // it doesn't matter if SDL_Init or Mix_Init failed, both report the errors through the same functions
            IrrlichtContext->getDevice ()->getLogger ()->log ("Cannot initialize SDL audio system", SDL_GetError(),irr::ELL_ERROR);
            return 2;
        }

        // initialize audio engine
        Mix_OpenAudio (22050, AUDIO_S16SYS, 2, 640);
    }

    WallpaperEngine::Core::CProject* project = WallpaperEngine::Core::CProject::fromFile (project_path);
    WallpaperEngine::Render::CWallpaper* wallpaper;

    if (strcmp (project->getType ().c_str (), "scene") == 0)
    {
        wallpaper = new WallpaperEngine::Render::CScene (project->getWallpaper ()->as <WallpaperEngine::Core::CScene> (), IrrlichtContext);
        IrrlichtContext->getDevice ()->getSceneManager ()->setAmbientLight (
                    wallpaper->getWallpaperData ()->as <WallpaperEngine::Core::CScene> ()->getAmbientColor ().toSColor ()
        );
    }
    else if (strcmp (project->getType ().c_str (), "video") == 0)
    {
        wallpaper = new WallpaperEngine::Render::CVideo (
                    project->getWallpaper ()->as <WallpaperEngine::Core::CVideo> (),
                    IrrlichtContext->getDevice ()->getVideoDriver ()
        );
    }

    irr::u32 minimumTime = 1000 / maximumFPS;
    irr::u32 startTime = 0;
    irr::u32 endTime = 0;

    while (IrrlichtContext && IrrlichtContext->getDevice () && IrrlichtContext->getDevice ()->run ())
    {
        if (IrrlichtContext->getDevice ()->getVideoDriver () == nullptr)
            continue;

        startTime = IrrlichtContext->getDevice ()->getTimer ()->getTime ();

        wallpaper->renderWallpaper ();

        endTime = IrrlichtContext->getDevice ()->getTimer ()->getTime ();

        IrrlichtContext->getDevice ()->sleep (minimumTime - (endTime - startTime), false);
    }

    SDL_Quit ();
    return 0;
}