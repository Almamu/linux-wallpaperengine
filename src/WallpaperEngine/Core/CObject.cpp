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
    auto id_it = data.find ("id");
    auto visible_it = data.find ("visible");
    auto origin_it = data.find ("origin");
    auto scale_it = data.find ("scale");
    auto angles_it = data.find ("angles");
    auto name_it = data.find ("name");
    auto effects_it = data.find ("effects");

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

    auto image_it = data.find ("image");
    auto sound_it = data.find ("sound");
    auto particle_it = data.find ("particle");

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
        auto cur = (*effects_it).begin ();
        auto end = (*effects_it).end ();

        for (; cur != end; cur ++)
        {
            object->insertEffect (
                    Objects::CEffect::fromJSON (*cur, object)
            );
        }
    }

    return object;
}

const irr::core::vector3df& CObject::getOrigin () const
{
    return this->m_origin;
}

const irr::core::vector3df& CObject::getScale () const
{
    return this->m_scale;
}

const irr::core::vector3df& CObject::getAngles () const
{
    return this->m_angles;
}

const std::string& CObject::getName () const
{
    return this->m_name;
}

const std::vector<Objects::CEffect*>& CObject::getEffects () const
{
    return this->m_effects;
}

bool CObject::isVisible ()
{
    return this->m_visible;
}

const int CObject::getId () const
{
    return this->m_id;
}

void CObject::insertEffect (Objects::CEffect* effect)
{
    this->m_effects.push_back (effect);
}