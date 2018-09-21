#include <iostream>
#include <irrlicht/irrlicht.h>
#include <sstream>
#include <wallpaperengine/config.h>
#include <wallpaperengine/video/renderer.h>
#include <wallpaperengine/video/material.h>

#include "wallpaperengine/shaders/compiler.h"
#include "wallpaperengine/project.h"
#include "wallpaperengine/irrlicht.h"

int WinID = 0;
irr::SIrrlichtCreationParameters _irr_params;

irr::f32 g_AnimationSpeed = 0.1f;
irr::f32 g_Scale = 2.5f;
irr::f32 g_ScrollSpeed = 0.0f;
irr::f32 g_Direction = 0.0f;
irr::f32 g_Strength = 0.07f;
irr::f32 g_SpecularPower = 1.0f;
irr::f32 g_SpecularStrength = 1.0f;
irr::f32 g_SpecularColor [3] = {1.0f, 1.0f, 1.0f};
irr::f32 g_Texture1Resolution [4] = {1.0f, 1.0f, 1.0f, 1.0f};
irr::f32 g_Texture0 = 0;
irr::f32 g_Texture1 = 1;
irr::f32 g_Texture2 = 2;
irr::f32 g_Time = 0;

class MyShaderCallback : public irr::video::IShaderConstantSetCallBack
{
    virtual void OnSetConstants (irr::video::IMaterialRendererServices* services, int32_t userData)
    {
        irr::video::IVideoDriver* driver = services->getVideoDriver ();

        irr::core::matrix4 worldViewProj;
        worldViewProj = driver->getTransform(irr::video::ETS_PROJECTION);
        worldViewProj *= driver->getTransform(irr::video::ETS_VIEW);
        worldViewProj *= driver->getTransform(irr::video::ETS_WORLD);

        services->setVertexShaderConstant ("g_AnimationSpeed", &g_AnimationSpeed, 1);
        services->setVertexShaderConstant ("g_Scale", &g_Scale, 1);
        services->setVertexShaderConstant ("g_ScrollSpeed", &g_ScrollSpeed, 1);
        services->setVertexShaderConstant ("g_Direction", &g_Direction, 1);
        services->setVertexShaderConstant ("g_Time", &g_Time, 1);
        services->setVertexShaderConstant ("g_ModelViewProjectionMatrix", worldViewProj.pointer(), 16);
        services->setVertexShaderConstant ("g_Texture0Resolution", g_Texture1Resolution, 4);
        services->setVertexShaderConstant ("g_Texture1Resolution", g_Texture1Resolution, 4);
        services->setVertexShaderConstant ("g_Texture2Resolution", g_Texture1Resolution, 4);

        // TODO: Support up to 7 materials (as wallpaper engine)
        services->setPixelShaderConstant ("g_Strength", &g_Strength, 1);
        services->setPixelShaderConstant ("g_SpecularPower", &g_SpecularPower, 1);
        services->setPixelShaderConstant ("g_SpecularStrength", &g_SpecularStrength, 1);
        services->setPixelShaderConstant ("g_SpecularColor", g_SpecularColor, 3);
        services->setPixelShaderConstant ("g_Texture0", &g_Texture0, 1);
        services->setPixelShaderConstant ("g_Texture1", &g_Texture1, 1);
        services->setPixelShaderConstant ("g_Texture2", &g_Texture2, 1);
    }
};

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

    return 0;
}

void preconfigure_wallpaper_engine ()
{
    wp::config::path::base = wp::irrlicht::device->getFileSystem ()->getAbsolutePath ("../");
    wp::config::path::resources = wp::config::path::base + "/res";
    wp::config::path::shaders = wp::config::path::resources + "/shaders";

    wp::fs::resolver.changeWorkingDirectory (wp::config::path::base);
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

    // set our working directory
    wp::fs::resolver.changeWorkingDirectory (_wp_engine_folder);
    wp::project* wp_project = new wp::project ();

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