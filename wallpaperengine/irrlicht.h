#ifndef WALLENGINE_IRRLICHT_H
#define WALLENGINE_IRRLICHT_H

#include <irrlicht/irrlicht.h>

namespace wp
{
    class irrlicht
    {
    public:
        static irr::video::IVideoDriver* driver;
        static irr::IrrlichtDevice* device;
        static irr::scene::ICameraSceneNode* camera;
    };
}


#endif //WALLENGINE_IRRLICHT_H
