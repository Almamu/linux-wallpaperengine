#include <iostream>
#include <irrlicht/irrlicht.h>
#include <sstream>
#include <wallpaperengine/video/renderer.h>
#include <wallpaperengine/video/material.h>
#include <wallpaperengine/irr/CPkgReader.h>
#include <getopt.h>

#include "wallpaperengine/shaders/compiler.h"
#include "wallpaperengine/project.h"
#include "wallpaperengine/irrlicht.h"
#include "wallpaperengine/irr/CImageLoaderTEX.h"

int WinID = 0;
irr::SIrrlichtCreationParameters _irr_params;

irr::f32 g_Time = 0;

int init_irrlicht()
{
    // prepare basic configuration for irrlicht
    _irr_params.AntiAlias = 8;
    _irr_params.Bits = 16;
    // _irr_params.DeviceType = irr::EIDT_X11;
    _irr_params.DriverType = irr::video::EDT_OPENGL;
    _irr_params.Doublebuffer = true;
    _irr_params.EventReceiver = nullptr;
    _irr_params.Fullscreen = false;
    _irr_params.HandleSRGB = false;
    _irr_params.IgnoreInput = true;
    _irr_params.Stencilbuffer = true;
    _irr_params.UsePerformanceTimer = false;
    _irr_params.Vsync = false;
    _irr_params.WithAlphaChannel = false;
    _irr_params.ZBufferBits = 24;
    _irr_params.LoggingLevel = irr::ELL_DEBUG;
    _irr_params.WindowId = reinterpret_cast<void*> (WinID);

    wp::irrlicht::device = irr::createDeviceEx (_irr_params);

    if (wp::irrlicht::device == nullptr)
    {
        return 1;
    }

    wp::irrlicht::device->setWindowCaption (L"Test game");
    wp::irrlicht::driver = wp::irrlicht::device->getVideoDriver();

    // check for ps and vs support
    if (
            wp::irrlicht::driver->queryFeature (irr::video::EVDF_PIXEL_SHADER_1_1) == false &&
            wp::irrlicht::driver->queryFeature (irr::video::EVDF_ARB_FRAGMENT_PROGRAM_1) == false)
    {
        wp::irrlicht::device->getLogger ()->log ("WARNING: Pixel shaders disabled because of missing driver/hardware support");
    }

    if (
            wp::irrlicht::driver->queryFeature (irr::video::EVDF_VERTEX_SHADER_1_1) == false &&
            wp::irrlicht::driver->queryFeature (irr::video::EVDF_ARB_VERTEX_PROGRAM_1) == false)
    {
        wp::irrlicht::device->getLogger ()->log ("WARNING: Vertex shaders disabled because of missing driver/hardware support");
    }

    return 0;
}

void preconfigure_wallpaper_engine ()
{
    // load the assets from wallpaper engine
    wp::irrlicht::device->getFileSystem ()->addFileArchive ("assets.zip", true, false);

    // register custom loaders
    wp::irrlicht::driver->addExternalImageLoader (new irr::video::CImageLoaderTex ());
    wp::irrlicht::device->getFileSystem ()->addArchiveLoader (new CArchiveLoaderPkg (wp::irrlicht::device->getFileSystem ()));
}

int main (int argc, char* argv[])
{
    int mode = 0;
    std::string path;

    // parse the integer if it exists
    if (argc >= 1)
    {
        std::stringstream ss;
        ss << std::hex << argv[1];
        ss >> WinID;
    }

    int option_index = 0;

    static struct option long_options [] = {
            {"win",     required_argument, 0, 'w'},
            {"pkg",     required_argument, 0, 'p'},
            {"dir",     required_argument, 0, 'd'},
            {nullptr,                   0, 0,   0}
    };

    while (true)
    {
        int c = getopt_long (argc, argv, "w:p:d:", long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 'w':
                if (optarg)
                    WinID = atoi (optarg);
                break;

            case 'p':
                mode = 1;
                path = optarg;
                break;

            case 'd':
                mode = 2;
                path = optarg;
                break;

            default:
                break;
        }
    }

    printf ("Initializing irrlicht to WindowID %d\n", WinID);

    if (init_irrlicht())
    {
        return 1;
    }

    preconfigure_wallpaper_engine ();

    irr::io::path wallpaper_path;
    irr::io::path project_path;
    irr::io::path scene_path;

    switch (mode)
    {
        // pkg mode
        case 1:
            wallpaper_path = wp::irrlicht::device->getFileSystem ()->getAbsolutePath (path.c_str ());
            project_path = wallpaper_path + "project.json";
            scene_path = wallpaper_path + "scene.pkg";

            wp::irrlicht::device->getFileSystem ()->addFileArchive (scene_path, true, false); // add the pkg file to the lookup list
            break;

        // folder mode
        case 2:
            wallpaper_path = wp::irrlicht::device->getFileSystem ()->getAbsolutePath (path.c_str ());
            project_path = wallpaper_path + "project.json";

            // set our working directory
            wp::irrlicht::device->getFileSystem ()->changeWorkingDirectoryTo (wallpaper_path);
            break;

        default:
            break;
    }

    wp::project* wp_project = new wp::project (project_path);

    if (wp_project->getScene ()->isOrthogonal() == true)
    {
        wp::video::renderer::setupOrthographicCamera (wp_project->getScene ());
    }
    else
    {
        wp::irrlicht::device->getLogger ()->log ("Non-orthogonal cameras not supported yet!!");
        return 0;
    }

    // register nodes
    wp::video::renderer::queueNode (wp_project->getScene ());

    int32_t lastTime = 0;
    int32_t minimumTime = 1000 / 90;
    int32_t currentTime = 0;

    while (wp::irrlicht::device->run () && wp::irrlicht::driver)
    {
        // if (device->isWindowActive ())
        {
            currentTime = wp::irrlicht::device->getTimer ()->getTime ();
            g_Time = currentTime / 1000.0f;

            if (currentTime - lastTime > minimumTime)
            {
                wp::video::renderer::render ();
                lastTime = currentTime;
            }
            else
            {
                wp::irrlicht::device->sleep (1, false);
            }
        }
    }

    delete wp_project;

    return 0;
}