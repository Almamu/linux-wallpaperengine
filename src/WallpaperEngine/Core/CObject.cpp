#include "CObject.h"

#include <utility>
#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/CSound.h"
#include "WallpaperEngine/Core/Objects/CParticle.h"

#include "Core.h"

using namespace WallpaperEngine::Core;

CObject::CObject (
        bool visible,
        irr::u32 id,
        std::string name,
        std::string type,
        const irr::core::vector3df& origin,
        const irr::core::vector3df& scale,
        const irr::core::vector3df& angles) :
    m_visible (visible),
    m_id (id),
    m_name (std::move(name)),
    m_type (type),
    m_origin (origin),
    m_scale (scale),
    m_angles (angles)
{
}

CObject* CObject::fromJSON (json data)
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
    json::const_iterator particle_it = data.find ("particle");

    CObject* object = nullptr;

    if (image_it != data.end () && (*image_it).is_null () == false)
    {
        object = Objects::CImage::fromJSON (
                data,
                visible,
                *id_it,
                *name_it,
                WallpaperEngine::Core::ato3vf (*origin_it),
                WallpaperEngine::Core::ato3vf (*scale_it),
                WallpaperEngine::Core::ato3vf (*angles_it)
        );
    }
    else if (sound_it != data.end ())
    {
        object = Objects::CSound::fromJSON (
                data,
                visible,
                *id_it,
                *name_it,
                WallpaperEngine::Core::ato3vf (*origin_it),
                WallpaperEngine::Core::ato3vf (*scale_it),
                WallpaperEngine::Core::ato3vf (*angles_it)
        );
    }
    else if (particle_it != data.end ())
    {
        object = Objects::CParticle::fromFile (
                (*particle_it).get <std::string> ().c_str (),
                *id_it,
                *name_it,
                WallpaperEngine::Core::ato3vf (*origin_it),
                WallpaperEngine::Core::ato3vf (*scale_it)
        );
    }
    else
    {
        throw std::runtime_error ("Unkonwn object type detected");
    }

    if (effects_it != data.end () && (*effects_it).is_array () == true)
    {
        json::const_iterator cur = (*effects_it).begin ();
        json::const_iterator end = (*effects_it).end ();

        for (; cur != end; cur ++)
        {
            object->insertEffect (
                    Objects::CEffect::fromJSON (*cur, object)
            );
        }
    }

    return object;
}

irr::core::vector3df* CObject::getOrigin ()
{
    return &this->m_origin;
}

irr::core::vector3df* CObject::getScale ()
{
    return &this->m_scale;
}

irr::core::vector3df* CObject::getAngles ()
{
    return &this->m_angles;
}

std::vector<Objects::CEffect*>* CObject::getEffects ()
{
    return &this->m_effects;
}

int CObject::getId ()
{
    return this->m_id;
}

void CObject::insertEffect (Objects::CEffect* effect)
{
    this->m_effects.push_back (effect);
}