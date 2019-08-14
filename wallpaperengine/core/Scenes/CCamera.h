#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace wp::core::Scenes
{
    using json = nlohmann::json;

    class CCamera
    {
    public:
        static CCamera* fromJSON (json data);

        irr::core::vector3df* getCenter ();
        irr::core::vector3df* getEye ();
        irr::core::vector3df* getUp ();
    protected:
        CCamera (irr::core::vector3df center, irr::core::vector3df eye, irr::core::vector3df up);
    private:
        irr::core::vector3df m_center;
        irr::core::vector3df m_eye;
        irr::core::vector3df m_up;
    };
};
