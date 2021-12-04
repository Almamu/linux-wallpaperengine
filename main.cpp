#include <iostream>
#include <getopt.h>
#include <unistd.h>
#include <SDL_mixer.h>
#include <SDL.h>
#include <FreeImage.h>

#include <GL/glew.h>
#include <GL/glx.h>
#include <filesystem>
#include "GLFW/glfw3.h"

#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/CContext.h"
#include "WallpaperEngine/Render/CScene.h"
#include "WallpaperEngine/Render/CVideo.h"

#include "WallpaperEngine/Assets/CPackage.h"
#include "WallpaperEngine/Assets/CDirectory.h"
#include "WallpaperEngine/Assets/CCombinedContainer.h"

enum BACKGROUND_RUN_MODE
{
    RUN_MODE_UNKNOWN = 0,
    RUN_MODE_HELP = 1,
    RUN_MODE_DIRECTORY = 2,
    RUN_MODE_PACKAGE = 3
};

float g_Time;

using namespace WallpaperEngine::Core::Types;

void print_help (const char* route)
{
    std::cout
        << "Usage:" << route << " [options] background_path" << std::endl
        << "options:" << std::endl
        << "  --silent\t\tMutes all the sound the wallpaper might produce" << std::endl
        << "  --screen-root <screen name>\tDisplay as screen's background" << std::endl
        << "  --fps <maximum-fps>\tLimits the FPS to the given number, useful to keep battery consumption low" << std::endl;
}

std::string stringPathFixes(const std::string& s)
{
    if (s.empty () == true)
        return s;

    std::string str (s);

    // remove single-quotes from the arguments
    if (str [0] == '\'' && str [str.size() - 1] == '\'')
        str
            .erase (str.size() - 1, 1)
            .erase (0, 1);

    // ensure there's a slash at the end of the path
    if (str [str.size() - 1] != '/')
        str += '/';

    return std::move (str);
}

int main (int argc, char* argv[])
{
    std::vector <std::string> screens;

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
                screens.emplace_back (optarg);
                break;

            case 'p':
            case 'd':
                std::cerr << "--dir/--pkg is deprecated and not used anymore" << std::endl;
                path = stringPathFixes (optarg);
                break;

            case 's':
                shouldEnableAudio = false;
                break;

            case 'h':
                print_help (argv [0]);
                break;

            case 'f':
                maximumFPS = atoi (optarg);
                break;

            default:
                break;
        }
    }

    // increment the option index (useful for when no options were found)
    option_index ++;

    if (path.empty () == true)
    {
        if (option_index < argc && strlen (argv [option_index]) > 0)
        {
            path = argv [option_index];
        }
        else
        {
            print_help (argv [0]);
            return 0;
        }
    }

    // first of all, initialize the window
    if (glfwInit () == GLFW_FALSE)
    {
        fprintf (stderr, "Failed to initialize GLFW\n");
        return 1;
    }

    // initialize freeimage
    FreeImage_Initialise (TRUE);

    // set some window hints (opengl version to be used)
    glfwWindowHint (GLFW_SAMPLES, 4);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 1);

    std::string project_path = path + "project.json";
    auto containers = new WallpaperEngine::Assets::CCombinedContainer ();

    // the background's path is required to load project.json regardless of the type of background we're using
    containers->add (new WallpaperEngine::Assets::CDirectory (path));
    // check if scene.pkg exists and add it to the list
    try
    {
        std::string scene_path = path + "scene.pkg";

        // add the package to the list
        containers->add (new WallpaperEngine::Assets::CPackage (scene_path));
    }
    catch(std::filesystem::filesystem_error ex)
    {
        // ignore this error, the package file was not found
    }
    catch (std::runtime_error ex)
    {
        // the package was found but there was an error loading it (wrong header or something)
        fprintf (stderr, "Failed to load scene.pkg file: %s\n", ex.what());
        return 4;
    }

    // add containers to the list
    containers->add (new WallpaperEngine::Assets::CDirectory ("./assets/"));

    // parse the project.json file
    auto project = WallpaperEngine::Core::CProject::fromFile ("project.json", containers);
    WallpaperEngine::Render::CWallpaper* wallpaper;
    // initialize custom context class
    WallpaperEngine::Render::CContext* context = new WallpaperEngine::Render::CContext (screens);

    // auto projection = project->getWallpaper ()->as <WallpaperEngine::Core::CScene> ()->getOrthogonalProjection ();
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

    // initialize inputs
    CMouseInput* mouseInput = new CMouseInput (window);

    context->setMouse (mouseInput);

    glfwMakeContextCurrent (window);

    // TODO: FIGURE THESE OUT BASED ON THE SCREEN
    int windowWidth = 1920;
    int windowHeight = 1080;

    // get the real framebuffer size
    glfwGetFramebufferSize (window, &windowWidth, &windowHeight);
    // set the default viewport
    context->setDefaultViewport ({0, 0, windowWidth, windowHeight});

    if (glewInit () != GLEW_OK)
    {
        fprintf (stderr, "Failed to initialize GLEW");
        glfwTerminate ();
        return 3;
    }


    if (project->getType () == "scene")
    {
        WallpaperEngine::Core::CScene* scene = project->getWallpaper ()->as <WallpaperEngine::Core::CScene> ();
        wallpaper = new WallpaperEngine::Render::CScene (scene, containers, context);
    }
    else if (project->getType () == "video")
    {
        // special steps, running a video needs a root directory change, files are not loaded from the container classes
        // as they're streamed from disk
        chdir (path.c_str ());

        WallpaperEngine::Core::CVideo* video = project->getWallpaper ()->as <WallpaperEngine::Core::CVideo> ();
        wallpaper = new WallpaperEngine::Render::CVideo (video, containers, context);
    }
    else
    {
        throw std::runtime_error ("Unsupported wallpaper type");
    }

    // ensure the context knows what wallpaper to render
    context->setWallpaper (wallpaper);

    if (shouldEnableAudio == true)
    {
        int mixer_flags = MIX_INIT_MP3 | MIX_INIT_FLAC | MIX_INIT_OGG;

        if (SDL_Init (SDL_INIT_AUDIO) < 0 || mixer_flags != Mix_Init (mixer_flags))
        {
            // Mix_GetError is an alias for SDL_GetError, so calling it directly will yield the correct result
            // it doesn't matter if SDL_Init or Mix_Init failed, both report the errors through the same functions
            fprintf (stderr, "Cannot initialize SDL audio system, SDL_GetError: %s", SDL_GetError ());
            return 2;
        }

        // initialize audio engine
        Mix_OpenAudio (22050, AUDIO_S16SYS, 2, 640);
    }

    // TODO: FIGURE OUT THE REQUIRED INPUT MODE, AS SOME WALLPAPERS USE THINGS LIKE MOUSE POSITION
    // glfwSetInputMode (window, GLFW_STICKY_KEYS, GL_TRUE);

    // enable depth text
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LESS);

    // cull anything that doesn't look at the camera (might be useful to disable in the future)
    glDisable (GL_CULL_FACE);

    clock_t minimumTime = 1000 / maximumFPS;
    clock_t startTime = 0;
    clock_t endTime = 0;

    while (glfwWindowShouldClose (window) == 0)
    {
        // get the real framebuffer size
        glfwGetFramebufferSize (window, &windowWidth, &windowHeight);
        // set the default viewport
        context->setDefaultViewport ({0, 0, windowWidth, windowHeight});
        // calculate the current time value
        g_Time = (float) glfwGetTime ();
        // get the start time of the frame
        startTime = clock ();
        // update our inputs first
        mouseInput->update ();
        // render the scene
        context->render ();
        // do buffer swapping
        glfwSwapBuffers (window);
        // poll for events (like closing the window)
        glfwPollEvents ();
        // get the end time of the frame
        endTime = clock ();

        // ensure the frame time is correct to not overrun FPS
        if ((endTime - startTime) < minimumTime)
            usleep (static_cast <unsigned int> ((static_cast <double> ((minimumTime - (endTime - startTime))) / CLOCKS_PER_SEC) * 1000));
    }

    // terminate gl
    glfwTerminate ();
    // terminate SDL
    SDL_Quit ();
    // terminate free image
    FreeImage_DeInitialise ();

    return 0;
}