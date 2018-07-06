#include <irrlicht/path.h>
#include "object3d.h"
#include "image.h"

namespace wp
{
    image::image (json json_data, irr::io::path basepath) : object3d (object3d::Type::Type_Image)
    {
        json::const_iterator visible = json_data.find ("visible");
        json::const_iterator file = json_data.find ("image");

        if (visible != json_data.end () && (*visible).is_boolean () == true)
        {
            this->m_visible = *visible;
        }

        if (file != json_data.end () && (*file).is_string () == true)
        {
            std::string file_s = *file;

            this->m_file = basepath + "/" + file_s.c_str ();
        }
    }
}