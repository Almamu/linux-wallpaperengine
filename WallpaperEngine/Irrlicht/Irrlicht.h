#pragma once

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Irrlicht
{
    extern irr::video::IVideoDriver* driver;
    extern irr::IrrlichtDevice* device;
    extern irr::scene::ICameraSceneNode* camera;
};