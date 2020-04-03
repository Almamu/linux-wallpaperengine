#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Scenes
{
    using json = nlohmann::json;

    class CCamera
    {
    public:
        CCamera ();
        static CCamera* fromJSON (json data);

        const irr::core::vector3df& getCenter () const;
        const irr::core::vector3df& getEye () const;
        const irr::core::vector3df& getUp () const;
    protected:
        CCamera (irr::core::vector3df center, irr::core::vector3df eye, irr::core::vector3df up);
    private:
        irr::core::vector3df m_center;
        irr::core::vector3df m_eye;
        irr::core::vector3df m_up;
    };
};
