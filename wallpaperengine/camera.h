#ifndef WALLENGINE_CAMERA_H
#define WALLENGINE_CAMERA_H

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

namespace wp
{
    using json = nlohmann::json;

    class camera
    {
    public:
        camera (json json_data);
    private:
        irr::core::vector3df m_center, m_eye, m_up;
    };
};

#endif //WALLENGINE_CAMERA_H
