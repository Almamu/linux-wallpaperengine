#include <irrlicht/irrlicht.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

#include "object.h"
#include "core.h"
#include "object3d.h"
#include "image.h"

namespace wp
{
    using json = nlohmann::json;

    object::object (json json_data)
    {
        json::const_iterator size = json_data.find ("size");
        json::const_iterator scale = json_data.find ("scale");
        json::const_iterator origin = json_data.find ("origin");

        if (size != json_data.end () && size.value ().is_string () == true)
        {
            std::string size_s = *size;

            this->m_size = core::ato2vf (size_s.c_str ());
        }

        if (scale != json_data.end () && scale.value ().is_string () == true)
        {
            std::string scale_s = *scale;

            this->m_scale = core::ato3vf (scale_s.c_str ());
        }

        if (origin != json_data.end () && origin.value ().is_string () == true)
        {
            std::string origin_s = *origin;

            this->m_origin = core::ato3vf (origin_s.c_str ());
        }

        json::const_iterator angles = json_data.find ("angles");
        json::const_iterator id = json_data.find ("id");
        json::const_iterator name = json_data.find ("name");

        if (angles != json_data.end () && (*angles).is_string () == true)
        {
            std::string angles_s = *angles;

            this->m_angles = core::ato3vf (angles_s.c_str ());
        }

        if (id != json_data.end () && (*id).is_null () == false)
        {
            this->m_id = *id;
        }

        if (name != json_data.end () && (*name).is_string () == true)
        {
            this->m_name = *name;
        }

        json::const_iterator image = json_data.find ("image");
        json::const_iterator model = json_data.find ("model");
        json::const_iterator particle = json_data.find ("particle");

        object3d::Type _type = object3d::Type::Type_None;

        if (image != json_data.end () && (*image).is_null () == false)
        {
            _type = object3d::Type::Type_Image;
        }

        if (model != json_data.end () && (*model).is_null () == false)
        {
            _type = object3d::Type::Type_Model;
        }

        if (particle != json_data.end () && (*particle).is_null () == false)
        {
            _type = object3d::Type::Type_Particle;
        }

        switch (_type)
        {
            case object3d::Type::Type_Image:
                this->m_object3d = new wp::image (json_data);
                break;

            case object3d::Type::Type_Model:
                break;

            case object3d::Type::Type_Particle:
                break;
        }

        json::const_iterator effects = json_data.find ("effects");

        if (effects != json_data.end () && (*effects).is_array () == true)
        {
            json::const_iterator cur = (*effects).begin ();
            json::const_iterator end = (*effects).end ();

            for (; cur != end; cur ++)
            {
                this->m_effects.push_back (new effect (*cur));
            }
        }
    }

    object::~object ()
    {
        if (this->m_object3d != nullptr)
        {
            delete this->m_object3d;
        }
    }
}