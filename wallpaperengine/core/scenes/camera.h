#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace wp::core::scenes
{
    using json = nlohmann::json;

    class camera
    {
    public:
        static camera* fromJSON (json data);

        irr::core::vector3df* getCenter ();
        irr::core::vector3df* getEye ();
        irr::core::vector3df* getUp ();
    protected:
        camera (irr::core::vector3df center, irr::core::vector3df eye, irr::core::vector3df up);
    private:
        irr::core::vector3df m_center;
        irr::core::vector3df m_eye;
        irr::core::vector3df m_up;
    };
};
