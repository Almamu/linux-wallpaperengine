#include <FreeImage.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <SDL.h>
#include <SDL_mixer.h>
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

#include "WallpaperEngine/Render/Drivers/COpenGLDriver.h"
#include "Steam/FileSystem/FileSystem.h"
#include "common.h"

#define WORKSHOP_APP_ID 431960
#define APP_DIRECTORY "wallpaper_engine"

float g_Time;
float g_TimeLast;
bool g_KeepRunning = true;
int g_AudioVolume = 15;

void print_help (const char* route)
{
    sLog.out ("Usage: ", route, " [options] background_path/background_id");
    sLog.out ("");
    sLog.out ("where background_path/background_id can be:");
    sLog.out ("\tthe ID of the background (for autodetection on your steam installation)");
    sLog.out ("\ta full path to the background's folder");
    sLog.out ("");
    sLog.out ("options:");
    sLog.out ("\t--silent\t\t\t\t\tMutes all the sound the wallpaper might produce");
    sLog.out ("\t--volume <amount>\t\t\tSets the volume for all the sounds in the background");
    sLog.out ("\t--screen-root <screen name>\tDisplay as screen's background");
    sLog.out ("\t--fps <maximum-fps>\t\t\tLimits the FPS to the given number, useful to keep battery consumption low");
    sLog.out ("\t--assets-dir <path>\t\t\tFolder where the assets are stored");
    sLog.out ("\t--screenshot\t\t\t\tTakes a screenshot of the background");
    sLog.out ("\t--list-properties\t\t\tList all the available properties and their possible values");
    sLog.out ("\t--set-property <name=value>\tOverrides the default value of the given property");
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

void addPkg (CCombinedContainer* containers, const std::filesystem::path& path, std::string pkgfile)
{
    try
    {
        auto scene_path = std::filesystem::path (path) / pkgfile;

        // add the package to the list
        containers->add (new WallpaperEngine::Assets::CPackage (scene_path));
        sLog.out ("Detected ", pkgfile, " file at ", scene_path, ". Adding to list of searchable paths");
    }
    catch (CPackageLoadException& ex)
    {
        // ignore this error, the package file was not found
        sLog.out ("No ", pkgfile, " file found at ", path, ". Defaulting to normal folder storage");
    }
    catch (std::runtime_error& ex)
    {
        // the package was found but there was an error loading it (wrong header or something)
        sLog.exception ("Failed to load scene.pkg file: ", ex.what());
    }
}

CVirtualContainer* buildVirtualContainer ()
{
    CVirtualContainer* container = new WallpaperEngine::Assets::CVirtualContainer ();

    //
    // Had to get a little creative with the effects to achieve the same bloom effect without any custom code
    // these virtual files are loaded by an image in the scene that takes current _rt_FullFrameBuffer and
    // applies the bloom effect to render it out to the screen
    //

    // add the effect file for screen bloom
    container->add (
        "effects/wpenginelinux/bloomeffect.json",
        "{"
        "\t\"name\":\"camerabloom_wpengine_linux\","
        "\t\"group\":\"wpengine_linux_camera\","
        "\t\"dependencies\":[],"
        "\t\"passes\":"
        "\t["
        "\t\t{"
        "\t\t\t\"material\": \"materials/util/downsample_quarter_bloom.json\","
        "\t\t\t\"target\": \"_rt_4FrameBuffer\","
        "\t\t\t\"bind\":"
        "\t\t\t["
        "\t\t\t\t{"
        "\t\t\t\t\t\"name\": \"_rt_FullFrameBuffer\","
        "\t\t\t\t\t\"index\": 0"
        "\t\t\t\t}"
        "\t\t\t]"
        "\t\t},"
        "\t\t{"
        "\t\t\t\"material\": \"materials/util/downsample_eighth_blur_v.json\","
        "\t\t\t\"target\": \"_rt_8FrameBuffer\","
        "\t\t\t\"bind\":"
        "\t\t\t["
        "\t\t\t\t{"
        "\t\t\t\t\t\"name\": \"_rt_4FrameBuffer\","
        "\t\t\t\t\t\"index\": 0"
        "\t\t\t\t}"
        "\t\t\t]"
        "\t\t},"
        "\t\t{"
        "\t\t\t\"material\": \"materials/util/blur_h_bloom.json\","
        "\t\t\t\"target\": \"_rt_Bloom\","
        "\t\t\t\"bind\":"
        "\t\t\t["
        "\t\t\t\t{"
        "\t\t\t\t\t\"name\": \"_rt_8FrameBuffer\","
        "\t\t\t\t\t\"index\": 0"
        "\t\t\t\t}"
        "\t\t\t]"
        "\t\t},"
        "\t\t{"
        "\t\t\t\"material\": \"materials/util/combine.json\","
        "\t\t\t\"target\": \"_rt_FullFrameBuffer\","
        "\t\t\t\"bind\":"
        "\t\t\t["
        "\t\t\t\t{"
        "\t\t\t\t\t\"name\": \"_rt_imageLayerComposite_-1_a\","
        "\t\t\t\t\t\"index\": 0"
        "\t\t\t\t},"
        "\t\t\t\t{"
        "\t\t\t\t\t\"name\": \"_rt_Bloom\","
        "\t\t\t\t\t\"index\": 1"
        "\t\t\t\t}"
        "\t\t\t]"
        "\t\t}"
        "\t]"
        "}"
    );

    // add some model for the image element even if it's going to waste rendering cycles
    container->add (
        "models/wpenginelinux.json",
        "{"
        "\t\"material\":\"materials/wpenginelinux.json\""
        "}"
    );

    // models require materials, so add that too
    container->add (
        "materials/wpenginelinux.json",
        "{"
        "\t\"passes\":"
        "\t\t["
        "\t\t\t{"
        "\t\t\t\t\"blending\": \"normal\","
        "\t\t\t\t\"cullmode\": \"nocull\","
        "\t\t\t\t\"depthtest\": \"disabled\","
        "\t\t\t\t\"depthwrite\": \"disabled\","
        "\t\t\t\t\"shader\": \"genericimage2\","
        "\t\t\t\t\"textures\": [\"_rt_FullFrameBuffer\"]"
        "\t\t\t}"
        "\t\t]"
        "}"
    );

    return container;
}

void takeScreenshot (WallpaperEngine::Render::CWallpaper* wp, const std::filesystem::path& filename, FREE_IMAGE_FORMAT format)
{
    GLint width, height;

    // bind texture and get the size
    glBindFramebuffer (GL_FRAMEBUFFER, wp->getWallpaperFramebuffer ());
    glBindTexture (GL_TEXTURE_2D, wp->getWallpaperTexture ());
    glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
    glGetTexLevelParameteriv (GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

    // make room for storing the pixel data
    uint8_t* buffer = new uint8_t [width * height * sizeof (uint8_t) * 3];
    uint8_t* pixel = buffer;

    // read the image into the buffer
    glReadPixels (0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, buffer);

    // build the output file with FreeImage
    FIBITMAP* bitmap = FreeImage_Allocate (width, height, 24);
    RGBQUAD color;

    // now get access to the pixels
    for (int y = height; y > 0; y --)
    {
        for (int x = 0; x < width; x ++)
        {
            color.rgbRed = *pixel ++;
            color.rgbGreen = *pixel ++;
            color.rgbBlue = *pixel ++;

            // set the pixel in the destination
            FreeImage_SetPixelColor (bitmap, x, y, &color);
        }
    }

    // finally save the file
    FreeImage_Save (format, bitmap, filename.c_str (), 0);

    // free all the used memory
    delete[] buffer;

    FreeImage_Unload (bitmap);

    // unbind the textures
    glBindTexture (GL_TEXTURE_2D, GL_NONE);
}

void initLogging ()
{
    sLog.addOutput (new std::ostream (std::cout.rdbuf ()));
    sLog.addError (new std::ostream (std::cerr.rdbuf ()));
}

int main (int argc, char* argv[])
{
    initLogging ();

    std::vector <std::string> screens;
    std::map <std::string, std::string> propertyOverrides;

    int maximumFPS = 30;
    bool shouldEnableAudio = true;
    bool shouldTakeScreenshot = false;
    bool shouldListPropertiesAndStop = false;
    FREE_IMAGE_FORMAT screenshotFormat = FIF_UNKNOWN;
    std::string path;
    std::filesystem::path assetsPath;
    std::filesystem::path screenshotPath;

    static struct option long_options [] = {
        {"screen-root",     required_argument, 0, 'r'},
        {"pkg",             required_argument, 0, 'p'},
        {"dir",             required_argument, 0, 'd'},
        {"silent",          no_argument,       0, 's'},
        {"volume",          required_argument, 0, 'v'},
        {"help",            no_argument,       0, 'h'},
        {"fps",             required_argument, 0, 'f'},
        {"assets-dir",      required_argument, 0, 'a'},
        {"screenshot",      required_argument, 0, 'c'},
        {"list-properties", no_argument,       0, 'l'},
        {"set-property",    required_argument, 0, 'o'},
        {nullptr,                           0, 0,   0}
    };

    while (true)
    {
        int c = getopt_long (argc, argv, "r:p:d:shf:a:", long_options, nullptr);

        if (c == -1)
            break;

        switch (c)
        {
            case 'o':
                {
                    std::string value = optarg;
                    std::string::size_type equals = value.find ('=');

                    // properties without value are treated as booleans for now
                    if (equals == std::string::npos)
                        propertyOverrides.insert_or_assign (value, "1");
                    else
                        propertyOverrides.insert_or_assign (
                            value.substr (0, equals),
                            value.substr (equals + 1)
                        );
                }
                break;

            case 'l':
                shouldListPropertiesAndStop = true;
                break;

            case 'r':
                screens.emplace_back (optarg);
                break;

            case 'p':
            case 'd':
                sLog.error ("--dir/--pkg is deprecated and not used anymore");
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

            case 'a': assetsPath = optarg;
                break;

            case 'v':
                g_AudioVolume = atoi (optarg);
                break;

            case 'c':
                shouldTakeScreenshot = true;
                screenshotPath = stringPathFixes (optarg);
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

    // validate screenshot file just to make sure
    if (shouldTakeScreenshot == true)
    {
        if (screenshotPath.has_extension () == false)
            sLog.exception ("Cannot determine screenshot format");

        std::string extension = screenshotPath.extension ();

        if (extension == ".bmp")
            screenshotFormat = FIF_BMP;
        else if (extension == ".png")
            screenshotFormat = FIF_PNG;
        else if (extension == ".jpg" || extension == ".jpeg")
            screenshotFormat = FIF_JPEG;
        else
            sLog.exception ("Cannot determine screenshot format, unknown extension ", extension);
    }

    // check if the background might be an ID and try to find the right path in the steam installation folder
    if (path.find ('/') == std::string::npos)
        path = Steam::FileSystem::workshopDirectory (WORKSHOP_APP_ID, path);

    WallpaperEngine::Assets::CCombinedContainer containers;
    // the background's path is required to load project.json regardless of the type of background we're using
    containers.add (new WallpaperEngine::Assets::CDirectory (path));
    // add the virtual container for mocked up files
    containers.add (buildVirtualContainer ());
    // try to add the common packages
    addPkg (&containers, path, "scene.pkg");
    addPkg (&containers, path, "gifscene.pkg");

    if (assetsPath.empty () == true)
    {
        try
        {
            assetsPath = Steam::FileSystem::appDirectory (APP_DIRECTORY, "assets");
        }
        catch (std::runtime_error)
        {
            // set current path as assets' folder
            std::filesystem::path directory = std::filesystem::canonical ("/proc/self/exe")
                .parent_path () / "assets";
        }
    }
    else
    {
        sLog.out ("Found wallpaper engine's assets at ", assetsPath, " based on --assets-dir parameter");
    }

    if (assetsPath.empty () == true)
        sLog.exception ("Cannot determine a valid path for the wallpaper engine assets");

    // add containers to the list
    containers.add (new WallpaperEngine::Assets::CDirectory (assetsPath));

    // parse the project.json file
    auto project = WallpaperEngine::Core::CProject::fromFile ("project.json", &containers);
    // go to the right folder so the videos will play
    if (project->getWallpaper ()->is <WallpaperEngine::Core::CVideo> () == true)
        chdir (path.c_str ());

    // show properties if required
    for (auto cur : project->getProperties ())
    {
        // update the value of the property
        auto override = propertyOverrides.find (cur->getName ());

        if (override != propertyOverrides.end ())
        {
            sLog.out ("Applying override value for ", cur->getName ());

            cur->update (override->second);
        }

        if (shouldListPropertiesAndStop)
            sLog.out (cur->dump ());
    }

    // halt if the list-properties option was specified
    if (shouldListPropertiesAndStop)
        return 0;

    // attach signals so if a stop is requested the X11 resources are freed and the program shutsdown gracefully
    std::signal(SIGINT, signalhandler);
    std::signal(SIGTERM, signalhandler);

    if (shouldEnableAudio == true && SDL_Init (SDL_INIT_AUDIO) < 0)
    {
        sLog.error ("Cannot initialize SDL audio system, SDL_GetError: ", SDL_GetError());
        sLog.error ("Continuing without audio support");
    }

    // initialize OpenGL driver
    WallpaperEngine::Render::Drivers::COpenGLDriver videoDriver ("Wallpaper Engine");
    // initialize custom context class
    WallpaperEngine::Render::CRenderContext context (screens, videoDriver, &containers);
    // initialize mouse support
    context.setMouse (new CMouseInput (videoDriver.getWindow ()));
    // ensure the context knows what wallpaper to render
    context.setWallpaper (
        WallpaperEngine::Render::CWallpaper::fromWallpaper (project->getWallpaper (), &context)
    );

    // update maximum FPS if it's a video
    if (context.getWallpaper ()->is <WallpaperEngine::Render::CVideo> () == true)
        maximumFPS = context.getWallpaper ()->as <WallpaperEngine::Render::CVideo> ()->getFPS ();

    float startTime, endTime, minimumTime = 1.0f / maximumFPS;

    while (videoDriver.closeRequested () == false && g_KeepRunning == true)
    {
        // keep track of the previous frame's time
        g_TimeLast = g_Time;
        // calculate the current time value
        g_Time = videoDriver.getRenderTime ();
        // get the start time of the frame
        startTime = g_Time;
        // render the scene
        context.render ();
        // get the end time of the frame
        endTime = videoDriver.getRenderTime ();

        // ensure the frame time is correct to not overrun FPS
        if ((endTime - startTime) < minimumTime)
            usleep ((minimumTime - (endTime - startTime)) * CLOCKS_PER_SEC);

        if (shouldTakeScreenshot == true && videoDriver.getFrameCounter () == 5)
        {
            takeScreenshot (context.getWallpaper (), screenshotPath, screenshotFormat);
            // disable screenshot just in case the counter overflows
            shouldTakeScreenshot = false;
        }
    }

    // ensure this is updated as sometimes it might not come from a signal
    g_KeepRunning = false;

    sLog.out ("Stop requested");

    // terminate SDL
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    SDL_Quit ();

    return 0;
}