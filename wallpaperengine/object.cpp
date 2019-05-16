#include <irrlicht/irrlicht.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

#include <wallpaperengine/object.h>
#include <wallpaperengine/core.h>
#include <wallpaperengine/object3d.h>
#include <wallpaperengine/image.h>

namespace wp
{
    using json = nlohmann::json;

    object::object (json json_data, wp::scene* scene) : m_object3d (nullptr)
    {
        this->m_scene = scene;

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
            _type = object3d::Type::Type_Material;
        }

        if (model != json_data.end () && (*model).is_null () == false)
        {
            _type = object3d::Type::Type_Model;
        }

        if (particle != json_data.end () && (*particle).is_null () == false)
        {
            _type = object3d::Type::Type_Particle;
        }

        // load the effects first so we have access to the textures needed
        json::const_iterator effects = json_data.find ("effects");

        if (effects != json_data.end () && (*effects).is_array () == true)
        {
            json::const_iterator cur = (*effects).begin ();
            json::const_iterator end = (*effects).end ();

            for (; cur != end; cur ++)
            {
                this->m_effects.push_back (new effect (*cur, this));
            }
        }

        switch (_type)
        {
            case object3d::Type::Type_Material:
                this->m_object3d = new wp::image (json_data, this);
                break;

            case object3d::Type::Type_Model:
                break;

            case object3d::Type::Type_Particle:
                break;
        }
    }

    object::~object ()
    {
        if (this->m_object3d != nullptr)
        {
            delete this->m_object3d;
        }
    }

    void object::render ()
    {
        if (this->m_object3d != nullptr)
        {
            this->m_object3d->render ();
        }
    }


    irr::core::vector2df& object::getSize ()
    {
        return this->m_size;
    }

    irr::core::vector3df& object::getScale ()
    {
        return this->m_scale;
    }

    irr::core::vector3df& object::getOrigin ()
    {
        return this->m_origin;
    }

    irr::core::vector3df& object::getAngles ()
    {
        return this->m_angles;
    }

    std::vector<effect*>& object::getEffects ()
    {
        return this->m_effects;
    }

    wp::scene* object::getScene ()
    {
        return this->m_scene;
    }
}