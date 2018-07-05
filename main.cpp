#include <iostream>
#include <irrlicht/irrlicht.h>
#include <sstream>
#include "nier_test.h"

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

irr::IrrlichtDevice* device = nullptr;
irr::video::IVideoDriver* driver = nullptr;
irr::SIrrlichtCreationParameters _irr_params;

int init_driver ()
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

    device = irr::createDeviceEx (_irr_params);

    if (device == nullptr)
    {
        return 1;
    }

    device->setWindowCaption (L"Test game");
    driver = device->getVideoDriver();

    return 0;
}

int game_test_main ()
{
    if (init_driver())
    {
        return 1;
    }

    nier_test ();

    return 0;
}