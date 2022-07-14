#include <iostream>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <SDL_mixer.h>
#include <SDL.h>
#include <FreeImage.h>
#include <sys/stat.h>
#include <GL/glew.h>
#include <csignal>
#include <GLFW/glfw3.h>
#include <libgen.h>

#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include "WallpaperEngine/Render/CContext.h"

#include "WallpaperEngine/Assets/CPackage.h"
#include "WallpaperEngine/Assets/CDirectory.h"
#include "WallpaperEngine/Assets/CCombinedContainer.h"
#include "WallpaperEngine/Assets/CPackageLoadException.h"

float g_Time;
bool g_KeepRunning = true;
int g_AudioVolume = 15;

using namespace WallpaperEngine::Core::Types;

const char* assets_default_paths [] = {
        ".steam/steam/steamapps/common/wallpaper_engine/assets",
        ".local/share/Steam/steamapps/common/wallpaper_engine/assets",
        nullptr
};

const char* backgrounds_default_paths [] = {
    ".local/share/Steam/steamapps/workshop/content/431960",
    nullptr
};

void print_help (const char* route)
{
    std::cout
        << "Usage: " << route << " [options] background_path/background_id" << std::endl
        << std::endl
        << "where background_path/background_id can be:" << std::endl
        << "\tthe ID of the background (for autodetection on your steam installation)" << std::endl
        << "\ta full path to the background's folder" << std::endl
        << std::endl
        << "options:" << std::endl
        << "  --silent\t\tMutes all the sound the wallpaper might produce" << std::endl
        << "  --volume <amount>\tSets the volume for all the sounds in the background" << std::endl
        << "  --screen-root <screen name>\tDisplay as screen's background" << std::endl
        << "  --fps <maximum-fps>\tLimits the FPS to the given number, useful to keep battery consumption low" << std::endl
        << "  --assets-dir <path>\tFolder where the assets are stored" << std::endl;
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

    return std::move (str);
}

void signalhandler(int sig)
{
    g_KeepRunning = false;
}

int validatePath(const char* path, std::string& final)
{
    char finalPath [PATH_MAX];
    char* pointer = realpath (path, finalPath);

    if (finalPath == nullptr)
        return errno;

    // ensure the path points to a folder
    struct stat pathinfo;

    if (stat (finalPath, &pathinfo) != 0)
        return errno;

    if (!S_ISDIR (pathinfo.st_mode))
        return ENOTDIR;

    final = finalPath;

    return 0;
}

std::string getHomePath ()
{
    char* home = getenv ("HOME");

    if (home == nullptr)
        throw std::runtime_error ("$HOME doesn't exist");

    std::string homepath;

    int error = validatePath (home, homepath);

    if (error == ENOTDIR)
        throw std::runtime_error ("Invalid user home path");
    else if (error == ENAMETOOLONG)
        throw std::runtime_error ("Cannot get user's home folder, path is too long");
    else if (error != 0)
        throw std::runtime_error ("Cannot find the home folder for the user");

    return homepath;
}

void initGLFW ()
{
    // first of all, initialize the window
    if (glfwInit () == GLFW_FALSE)
        throw std::runtime_error ("Failed to initialize GLFW");

    // initialize freeimage
    FreeImage_Initialise (TRUE);

    // set some window hints (opengl version to be used)
    glfwWindowHint (GLFW_SAMPLES, 4);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 1);
}

void addPkg (CCombinedContainer* containers, const std::string& path, std::string pkgfile)
{
    try
    {
        std::string scene_path = path + pkgfile;

        // add the package to the list
        containers->add (new WallpaperEngine::Assets::CPackage (scene_path));
        std::cout << "Detected " << pkgfile << " file at " << scene_path << ". Adding to list of searchable paths" << std::endl;
    }
    catch (CPackageLoadException& ex)
    {
        // ignore this error, the package file was not found
        std::cout << "No " << pkgfile <<  " file found at " << path << ". Defaulting to normal folder storage" << std::endl;
    }
    catch (std::runtime_error& ex)
    {
        // the package was found but there was an error loading it (wrong header or something)
        fprintf (stderr, "Failed to load scene.pkg file: %s\n", ex.what());
        throw std::runtime_error ("Cannot load package file");
    }
}

int main (int argc, char* argv[])
{
    std::vector <std::string> screens;

    int maximumFPS = 30;
    bool shouldEnableAudio = true;
    std::string path;
    std::string assetsDir;

    static struct option long_options [] = {
        {"screen-root", required_argument, 0, 'r'},
        {"pkg",         required_argument, 0, 'p'},
        {"dir",         required_argument, 0, 'd'},
        {"silent",      no_argument,       0, 's'},
        {"volume",      required_argument, 0, 'v'},
        {"help",        no_argument,       0, 'h'},
        {"fps",         required_argument, 0, 'f'},
        {"assets-dir",  required_argument, 0, 'a'},
        {nullptr,                       0, 0,   0}
    };

    while (true)
    {
        int c = getopt_long (argc, argv, "r:p:d:shf:a:", long_options, nullptr);

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

            case 'a':
                assetsDir = optarg;
                break;

            case 'v':
                g_AudioVolume = atoi (optarg);
                break;
        }
    }

    if (path.empty () == true)
    {
        if (optind < argc && strlen (argv [optind]) > 0)
        {
            path = argv [optind];
        }
        else
        {
            print_help (argv [0]);
            return 0;
        }
    }

    // attach signals so if a stop is requested the X11 resources are freed and the program shutsdown gracefully
    std::signal(SIGINT, signalhandler);
    std::signal(SIGTERM, signalhandler);

    // initialize glfw
    initGLFW ();

    std::string homepath = getHomePath ();
    auto containers = new WallpaperEngine::Assets::CCombinedContainer ();

    // check if the background might be an ID and try to find the right path in the steam installation folder
    if (path.find ('/') == std::string::npos)
    {
        for (const char** current = backgrounds_default_paths; *current != nullptr; current ++)
        {
            std::string tmppath = homepath + "/" + *current + "/" + path;

            int error = validatePath (tmppath.c_str (), tmppath);

            if (error != 0)
                continue;

            path = tmppath;
        }
    }

    int error = validatePath (path.c_str (), path);

    if (error == ENOTDIR)
        throw std::runtime_error ("The background path is not a folder");
    else if (error == ENAMETOOLONG)
        throw std::runtime_error ("Cannot get wallpaper's folder, path is too long");
    else if (error != 0)
        throw std::runtime_error ("Cannot find the specified folder");

    // add a trailing slash to the path so the right file can be found
    path += "/";

    // the background's path is required to load project.json regardless of the type of background we're using
    containers->add (new WallpaperEngine::Assets::CDirectory (path));
    // try to add the common packages
    addPkg (containers, path, "scene.pkg");
    addPkg (containers, path, "gifscene.pkg");

    if (assetsDir.empty () == true)
    {
        for (const char** current = assets_default_paths; *current != nullptr; current ++)
        {
            std::string tmppath = homepath + "/" + *current;

            error = validatePath (tmppath.c_str (), tmppath);

            if (error != 0)
                continue;

            assetsDir = tmppath;
            std::cout << "Found wallpaper engine's assets at " << assetsDir << std::endl;
            break;
        }

        if (assetsDir.empty () == true)
        {
            unsigned long len = strlen (argv [0]) + 1;
            char* copy = new char[len];

            strncpy (copy, argv [0], len);

            // path still not found, try one last thing on the current binary's folder
            std::string exepath = dirname (copy);
            exepath += "/assets";

            error = validatePath (exepath.c_str (), exepath);

            if (error == 0)
            {
                assetsDir = exepath;
                std::cout << "Found assets folder alongside the binary: " << assetsDir << std::endl;
            }

            delete[] copy;
        }
    }
    else
    {
        error = validatePath (assetsDir.c_str (), assetsDir);

        if (error == ENOTDIR)
            throw std::runtime_error ("Invalid assets folder");
        else if (error == ENAMETOOLONG)
            throw std::runtime_error ("Cannot get assets folder, path is too long");
        else if (error != 0)
            throw std::runtime_error ("Cannot find the specified assets folder");

        std::cout << "Found wallpaper engine's assets at " << assetsDir << " based on --assets-dir parameter" << std::endl;
    }

    if (assetsDir.empty () == true)
        throw std::runtime_error ("Cannot determine a valid path for the wallpaper engine assets");

    // add containers to the list
    containers->add (new WallpaperEngine::Assets::CDirectory (assetsDir + "/"));

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

    glfwMakeContextCurrent (window);

    // TODO: FIGURE THESE OUT BASED ON THE SCREEN
    int windowWidth = 1920;
    int windowHeight = 1080;

    // get the real framebuffer size
    glfwGetFramebufferSize (window, &windowWidth, &windowHeight);

    // initialize glew
    if (glewInit () != GLEW_OK)
    {
        fprintf (stderr, "Failed to initialize GLEW");
        glfwTerminate ();
        return 3;
    }

    if (shouldEnableAudio == true && SDL_Init (SDL_INIT_AUDIO) < 0)
    {
        std::cerr << "Cannot initialize SDL audio system, SDL_GetError: " << SDL_GetError() << std::endl;
        std::cerr << "Continuing without audio support" << std::endl;
    }

    // parse the project.json file
    auto project = WallpaperEngine::Core::CProject::fromFile ("project.json", containers);
    // go to the right folder so the videos will play
    if (project->getWallpaper ()->is <WallpaperEngine::Core::CVideo> () == true)
        chdir (path.c_str ());

    // initialize custom context class
    WallpaperEngine::Render::CContext* context = new WallpaperEngine::Render::CContext (screens, window);
    // initialize mouse support
    context->setMouse (new CMouseInput (window));
    // set the default viewport
    context->setDefaultViewport ({0, 0, windowWidth, windowHeight});
    // ensure the context knows what wallpaper to render
    context->setWallpaper (
        WallpaperEngine::Render::CWallpaper::fromWallpaper (project->getWallpaper (), containers, context)
    );

    // TODO: FIGURE OUT THE REQUIRED INPUT MODE, AS SOME WALLPAPERS USE THINGS LIKE MOUSE POSITION
    // glfwSetInputMode (window, GLFW_STICKY_KEYS, GL_TRUE);

    double startTime, endTime, minimumTime = 1.0 / maximumFPS;

    while (glfwWindowShouldClose (window) == 0 && g_KeepRunning == true)
    {
        // get the real framebuffer size
        glfwGetFramebufferSize (window, &windowWidth, &windowHeight);
        // set the default viewport
        context->setDefaultViewport ({0, 0, windowWidth, windowHeight});
        // calculate the current time value
        g_Time = (float) glfwGetTime ();
        // get the start time of the frame
        startTime = glfwGetTime ();
        // render the scene
        context->render ();
        // do buffer swapping
        glfwSwapBuffers (window);
        // poll for events (like closing the window)
        glfwPollEvents ();
        // get the end time of the frame
        endTime = glfwGetTime ();

        // ensure the frame time is correct to not overrun FPS
        if ((endTime - startTime) < minimumTime)
            usleep ((minimumTime - (endTime - startTime)) * CLOCKS_PER_SEC);
    }

    // ensure this is updated as sometimes it might not come from a signal
    g_KeepRunning = false;

    std::cout << "Stop requested" << std::endl;

    // terminate gl
    glfwTerminate ();
    // terminate free image
    FreeImage_DeInitialise ();
    // terminate SDL
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_Quit ();

    // free context
    delete context;

    return 0;
}