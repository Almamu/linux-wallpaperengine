#include <iostream>
#include <irrlicht/irrlicht.h>
#include <sstream>
#include <wallpaperengine/config.h>
#include <wallpaperengine/fs/fileResolver.h>
#include "nier_test.h"
#include "wallpaperengine/irrlicht.h"

int WinID = 0;

int game_test_main();

int main(int argc, char* argv[])
{
    // parse the integer if it exists
    if (argc >= 1)
    {
        std::stringstream ss;
        ss << std::hex << argv[1];
        ss >> WinID;
    }

    printf("Initializing X11 to %d\n", WinID);

    return game_test_main ();
}

irr::SIrrlichtCreationParameters _irr_params;

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

    wp::fs::resolver.changeWorkingDirectory(wp::config::path::base);
}

int game_test_main ()
{
    if (init_irrlicht())
    {
        return 1;
    }

    preconfigure_wallpaper_engine ();
    nier_test ();

    return 0;
}