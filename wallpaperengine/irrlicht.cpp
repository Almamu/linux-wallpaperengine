#include <wallpaperengine/irrlicht.h>

namespace wp
{
    irr::video::IVideoDriver* irrlicht::driver = nullptr;
    irr::IrrlichtDevice* irrlicht::device = nullptr;
    irr::scene::ICameraSceneNode* irrlicht::camera = nullptr;
}