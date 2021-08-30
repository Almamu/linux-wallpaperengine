#include <iostream>
#include <irrlicht/irrlicht.h>
#include <getopt.h>
#include <unistd.h>
#include <SDL_mixer.h>
#include <SDL.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Irrlicht/CContext.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/CScene.h"
#include "WallpaperEngine/Render/CVideo.h"

#include "WallpaperEngine/Assets/CPackage.h"
#include "WallpaperEngine/Assets/CDirectory.h"
#include "WallpaperEngine/Assets/CCombinedContainer.h"

#include "WallpaperEngine/Core/Types/FloatColor.h"

enum BACKGROUND_RUN_MODE
{
    RUN_MODE_UNKNOWN = 0,
    RUN_MODE_HELP = 1,
    RUN_MODE_DIRECTORY = 2,
    RUN_MODE_PACKAGE = 3
};

WallpaperEngine::Irrlicht::CContext* IrrlichtContext = nullptr;
double g_Time;

using namespace WallpaperEngine::Core::Types;

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

    // first of all, initialize the window
    if (glfwInit () == GLFW_FALSE)
    {
        fprintf (stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    // set some window hints (opengl version to be used)
    glfwWindowHint (GLFW_SAMPLES, 4);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 1);

    // create the window!
    // TODO: DO WE NEED TO PASS MONITOR HERE OR ANYTHING?
    // TODO: FIGURE OUT HOW TO PUT THIS WINDOW IN THE BACKGROUND
    GLFWwindow* window = glfwCreateWindow (1920, 1080, "WallpaperEngine", NULL, NULL);

    if (window == nullptr)
    {
        fprintf (stderr, "Failed to open a GLFW window");
        glfwTerminate ();
        return 2;
    }

    glfwMakeContextCurrent (window);

    int windowWidth = 1920;
    int windowHeight = 1080;

    // get the real framebuffer size
    glfwGetFramebufferSize (window, &windowWidth, &windowHeight);

    if (glewInit () != GLEW_OK)
    {
        fprintf (stderr, "Failed to initialize GLEW");
        glfwTerminate ();
        return 3;
    }

    std::string project_path = path + "project.json";
    auto containers = new WallpaperEngine::Assets::CCombinedContainer ();

    // add containers to the list
    containers->add (new WallpaperEngine::Assets::CDirectory ("./assets/"));
    // the background's path is required to load project.json regardless of the type of background we're using
    containers->add (new WallpaperEngine::Assets::CDirectory (path));

    if (mode == RUN_MODE_PACKAGE)
    {
        std::string scene_path = path + "scene.pkg";

        // add the package to the list
        containers->add (new WallpaperEngine::Assets::CPackage (scene_path));
    }
    else if (mode == RUN_MODE_DIRECTORY)
    {
        // nothing to do here anymore
    }

    // parse the project.json file
    auto project = WallpaperEngine::Core::CProject::fromFile ("project.json", containers);
    WallpaperEngine::Render::CWallpaper* wallpaper;

    if (project->getType () == "scene")
    {
        WallpaperEngine::Core::CScene* scene = project->getWallpaper ()->as <WallpaperEngine::Core::CScene> ();
        wallpaper = new WallpaperEngine::Render::CScene (scene, containers);
        // TODO: BUILD THE SCENE
    }
    else if (project->getType () == "video")
    {
        // TODO: BUILD THE VIDEO OBJECT
    }
    else
    {
        throw std::runtime_error ("Unsupported wallpaper type");
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

    // TODO: FIGURE OUT THE REQUIRED INPUT MODE, AS SOME WALLPAPERS USE THINGS LIKE MOUSE POSITION
    // glfwSetInputMode (window, GLFW_STICKY_KEYS, GL_TRUE);

    // set the scene clear color
    auto sceneInformation = project->getWallpaper ()->as <WallpaperEngine::Core::CScene> ();
    FloatColor clearColor = sceneInformation->getClearColor ();

    glClearColor (clearColor.r, clearColor.g, clearColor.b, clearColor.a);

    // enable depth text
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LESS);

    // cull anything that doesn't look at the camera (might be useful to disable in the future)
    //glEnable (GL_CULL_FACE);

    clock_t minimumTime = 1000 / maximumFPS;
    clock_t startTime = 0;
    clock_t endTime = 0;

    while (glfwWindowShouldClose (window) == 0)
    {
        // calculate the current time value
        g_Time += static_cast <double> (endTime - startTime) / CLOCKS_PER_SEC;
        // get the start time of the frame
        startTime = clock ();

        // do not use any framebuffer for now
        glBindFramebuffer (GL_FRAMEBUFFER, 0);
        // ensure we render over the whole screen
        glViewport (0, 0, 1920, 1080);

        // clear window
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // render the scene
        wallpaper->render ();

        // do buffer swapping
        glfwSwapBuffers (window);
        // poll for events (like closing the window)
        glfwPollEvents ();
        // get the end time of the frame
        endTime = clock ();

        // ensure the frame time is correct to not overrun FPS
        if ((endTime - startTime) < minimumTime)
            usleep (static_cast <unsigned int> ((static_cast <double> (endTime - startTime) / CLOCKS_PER_SEC) * 1000));
    }

    // terminate gl
    glfwTerminate ();
    // terminate SDL
    SDL_Quit ();

    return 0;
/*
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

    WallpaperEngine::Core::CProject* project = WallpaperEngine::Core::CProject::fromFile (project_path);
    WallpaperEngine::Render::CWallpaper* wallpaper;

    if (project->getType () == "scene")
    {
        WallpaperEngine::Core::CScene* scene = project->getWallpaper ()->as <WallpaperEngine::Core::CScene> ();
        wallpaper = new WallpaperEngine::Render::CScene (scene, IrrlichtContext);
        IrrlichtContext->getDevice ()->getSceneManager ()->setAmbientLight (
                    scene->getAmbientColor ().toSColor ()
        );
    }
    else if (project->getType () == "video")
    {
        wallpaper = new WallpaperEngine::Render::CVideo (
                    project->getWallpaper ()->as <WallpaperEngine::Core::CVideo> (),
                    IrrlichtContext
        );
    }
    else
    {
        throw std::runtime_error ("Unsupported wallpaper type");
    }
    
    irr::u32 minimumTime = 1000 / maximumFPS;
    irr::u32 startTime = 0;
    irr::u32 endTime = 0;

    while (IrrlichtContext && IrrlichtContext->getDevice () && IrrlichtContext->getDevice ()->run ())
    {
        if (IrrlichtContext->getDevice ()->getVideoDriver () == nullptr)
            continue;

        startTime = IrrlichtContext->getDevice ()->getTimer ()->getTime ();
        g_Time = startTime / 1000.0f;

        IrrlichtContext->renderFrame (wallpaper);

        endTime = IrrlichtContext->getDevice ()->getTimer ()->getTime ();

        IrrlichtContext->getDevice ()->sleep (minimumTime - (endTime - startTime), false);
    }

    SDL_Quit ();
    return 0;*/
}