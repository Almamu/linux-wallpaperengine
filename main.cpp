#include <iostream>
#include <irrlicht/irrlicht.h>
#include <sstream>
#include <wallpaperengine/config.h>
#include <wallpaperengine/video/renderer.h>
#include <wallpaperengine/video/material.h>
#include <wallpaperengine/irr/CPkgReader.h>

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
    wp::config::path::base = wp::irrlicht::device->getFileSystem ()->getAbsolutePath ("../");
    wp::config::path::resources = wp::config::path::base + "/res";
    wp::config::path::shaders = wp::config::path::resources + "/shaders";
}

int main (int argc, char* argv[])
{
    // parse the integer if it exists
    if (argc >= 1)
    {
        std::stringstream ss;
        ss << std::hex << argv[1];
        ss >> WinID;
    }

    printf ("Initializing X11 to %d\n", WinID);

    if (init_irrlicht())
    {
        return 1;
    }

    preconfigure_wallpaper_engine ();

    // do_decompress ();
    irr::io::path _wp_engine_folder = "/home/almamu/Development/tmp/nier__automata_-_become_as_gods_edition/";
    irr::io::path _wp_project_file = _wp_engine_folder + "project.json";

    // load the assets folder from wallpaper engine
    wp::irrlicht::device->getFileSystem ()->addFileArchive ("../assets.zip", true, false);

    // set our working directory
    wp::irrlicht::device->getFileSystem ()->changeWorkingDirectoryTo (_wp_engine_folder);

    // register custom loader
    wp::irrlicht::driver->addExternalImageLoader (new irr::video::CImageLoaderTex ());
    wp::irrlicht::device->getFileSystem ()->addArchiveLoader (new CArchiveLoaderPkg (wp::irrlicht::device->getFileSystem ()));
    // wp::irrlicht::device->getFileSystem ()->addFileArchive (_wp_engine_folder + "scene.pkg", true, false); // add the pkg file to the lookup list

    wp::project* wp_project = new wp::project (_wp_project_file);

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

    return 0;
}