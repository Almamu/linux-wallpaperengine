#pragma once

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Irrlicht
{
    class CContext
    {
    public:
        void setDevice (irr::IrrlichtDevice* device);

        irr::IrrlichtDevice* getDevice ();
    private:
        irr::IrrlichtDevice* m_device;
    };
};
