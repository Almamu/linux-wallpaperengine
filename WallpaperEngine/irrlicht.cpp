#include <WallpaperEngine/irrlicht.h>

namespace WallpaperEngine
{
    irr::video::IVideoDriver* irrlicht::driver = nullptr;
    irr::IrrlichtDevice* irrlicht::device = nullptr;
    irr::scene::ICameraSceneNode* irrlicht::camera = nullptr;
}