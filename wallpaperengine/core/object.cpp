#include "object.h"
#include "objects/image.h"
#include "objects/sound.h"

#include "../core.h"

using namespace wp::core;


object::object (
        bool visible,
        irr::u32 id,
        std::string name,
        const irr::core::vector3df& origin,
        const irr::core::vector3df& scale,
        const irr::core::vector3df& angles) :
    m_visible (visible),
    m_id (id),
    m_name (name),
    m_origin (origin),
    m_scale (scale),
    m_angles (angles)
{
}

object* object::fromJSON (json data)
{
    json::const_iterator id_it = data.find ("id");
    json::const_iterator visible_it = data.find ("visible");
    json::const_iterator origin_it = data.find ("origin");
    json::const_iterator scale_it = data.find ("scale");
    json::const_iterator size_it = data.find ("size");
    json::const_iterator angles_it = data.find ("angles");
    json::const_iterator name_it = data.find ("name");
    json::const_iterator effects_it = data.find ("effects");

    bool visible = true;

    if (id_it == data.end ())
    {
        throw std::runtime_error ("Objects must have id");
    }

    if (origin_it == data.end ())
    {
        throw std::runtime_error ("Objects must have origin point");
    }

    if (scale_it == data.end ())
    {
        throw std::runtime_error ("Objects must have scale");
    }

    if (angles_it == data.end ())
    {
        throw std::runtime_error ("Objects must have angles");
    }

    if (name_it == data.end ())
    {
        throw std::runtime_error ("Objects must have name");
    }

    // visibility is optional
    if (visible_it != data.end ())
    {
        visible = *visible_it;
    }

    json::const_iterator image_it = data.find ("image");
    json::const_iterator sound_it = data.find ("sound");

    object* object = nullptr;

    if (image_it != data.end () && (*image_it).is_null () == false)
    {
        object = objects::image::fromJSON (
            data,
            visible,
            *id_it,
            *name_it,
            wp::core::ato3vf (*origin_it),
            wp::core::ato3vf (*scale_it),
            wp::core::ato3vf (*angles_it)
        );
    }
    else if (sound_it != data.end ())
    {
        object = objects::sound::fromJSON (
            data,
            visible,
            *id_it,
            *name_it,
            wp::core::ato3vf (*origin_it),
            wp::core::ato3vf (*scale_it),
            wp::core::ato3vf (*angles_it)
        );
    }

    if (effects_it != data.end () && (*effects_it).is_array () == true && object != nullptr)
    {
        json::const_iterator cur = (*effects_it).begin ();
        json::const_iterator end = (*effects_it).end ();

        for (; cur != end; cur ++)
        {
            object->insertEffect (
                objects::effect::fromJSON (*cur)
            );
        }
    }

    return object;
}

std::vector<objects::effect*>* object::getEffects ()
{
    return &this->m_effects;
}

void object::insertEffect (objects::effect* effect)
{
    this->m_effects.push_back (effect);
}