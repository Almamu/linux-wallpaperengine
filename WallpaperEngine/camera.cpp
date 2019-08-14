#include "camera.h"
#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine
{
    camera::camera (json json_data)
    {
        json::const_iterator center_it = json_data.find ("center");
        json::const_iterator eye_it = json_data.find ("eye");
        json::const_iterator up_it = json_data .find ("up");

        if (center_it != json_data.end ())
        {
            // get center value first
            std::string center = *center_it;

            this->m_center = Core::ato3vf(center.c_str());
        }

        if (eye_it != json_data.end ())
        {
            std::string eye = *eye_it;

            this->m_eye = Core::ato3vf(eye.c_str());
        }

        if (up_it != json_data.end ())
        {
            std::string up = *up_it;

            this->m_up = Core::ato3vf(up.c_str());
        }
    }

    irr::core::vector3df camera::getCenter ()
    {
        return this->m_center;
    }

    irr::core::vector3df camera::getEye ()
    {
        return this->m_eye;
    }

    irr::core::vector3df camera::getUp ()
    {
        return this->m_up;
    }
}