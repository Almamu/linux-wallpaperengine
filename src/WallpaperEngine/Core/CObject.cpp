#include "CObject.h"

#include <utility>
#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/CSound.h"
#include "WallpaperEngine/Core/Objects/CParticle.h"

#include "WallpaperEngine/Assets/CContainer.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Assets;

CObject::CObject (
        bool visible,
        uint32_t id,
        std::string name,
        std::string type,
        const glm::vec3& origin,
        const glm::vec3& scale,
        const glm::vec3& angles) :
    m_visible (visible),
    m_id (id),
    m_name (std::move(name)),
    m_type (std::move(type)),
    m_origin (origin),
    m_scale (scale),
    m_angles (angles)
{
}

CObject* CObject::fromJSON (json data, CContainer* container)
{
    std::string json = data.dump ();

    auto id_it = jsonFindRequired (data, "id", "Objects must have id");
    auto visible_it = data.find ("visible");
    auto origin_val = jsonFindDefault <std::string> (data, "origin", "0.0 0.0 0.0");
    auto scale_val = jsonFindDefault <std::string> (data, "scale", "0.0 0.0 0.0");
    auto angles_val = jsonFindDefault <std::string> (data, "angles", "0.0 0.0 0.0");
    auto name_it = jsonFindRequired (data, "name", "Objects must have name");
    auto effects_it = data.find ("effects");
    auto dependencies_it = data.find ("dependencies");

    bool visible = true;

    // visibility is optional
    if (visible_it != data.end ())
    {
        if (visible_it->is_boolean () == false)
        {
            // TODO: SUPPORT CONFIGURATION VALUES ON ATTRIBUTES LIKE VISIBLE
            // TODO: FOR NOW JUST DEFAULT TO FALSE
            visible = false;
        }
        else
        {
            visible = *visible_it;
        }
    }

    auto image_it = data.find ("image");
    auto sound_it = data.find ("sound");
    auto particle_it = data.find ("particle");
    auto text_it = data.find ("text");
    auto light_it = data.find ("light");

    CObject* object = nullptr;

    if (image_it != data.end () && (*image_it).is_null () == false)
    {
        object = Objects::CImage::fromJSON (
                data,
                container,
                visible,
                *id_it,
                *name_it,
                WallpaperEngine::Core::aToVector3 (origin_val),
                WallpaperEngine::Core::aToVector3 (scale_val),
                WallpaperEngine::Core::aToVector3 (angles_val)
        );
    }
    else if (sound_it != data.end () && (*sound_it).is_null () == false)
    {
        object = Objects::CSound::fromJSON (
                data,
                visible,
                *id_it,
                *name_it,
                WallpaperEngine::Core::aToVector3 (origin_val),
                WallpaperEngine::Core::aToVector3 (scale_val),
                WallpaperEngine::Core::aToVector3 (angles_val)
        );
    }
    else if (particle_it != data.end () && (*particle_it).is_null () == false)
    {
        /// TODO: XXXHACK -- TO REMOVE WHEN PARTICLE SUPPORT IS PROPERLY IMPLEMENTED
        try
        {
            object = Objects::CParticle::fromFile (
                (*particle_it).get <std::string> (),
                container,
                *id_it,
                *name_it,
                WallpaperEngine::Core::aToVector3 (origin_val),
                WallpaperEngine::Core::aToVector3 (scale_val)
            );
        }
        catch (std::runtime_error ex)
        {
            return nullptr;
        }
    }
    else if (text_it != data.end () && (*text_it).is_null () == false)
    {
        /// TODO: XXXHACK -- TO REMOVE WHEN TEXT SUPPORT IS IMPLEMENTED
        return nullptr;
    }
    else if (light_it != data.end () && (*light_it).is_null () == false)
    {
        /// TODO: XXXHACK -- TO REMOVE WHEN LIGHT SUPPORT IS IMPLEMENTED
        return nullptr;
    }
    else
    {
        throw std::runtime_error (std::string ("Unkonwn object type detected ").append (*name_it));
    }

    if (effects_it != data.end () && (*effects_it).is_array () == true)
    {
        auto cur = (*effects_it).begin ();
        auto end = (*effects_it).end ();

        for (; cur != end; cur ++)
        {
            object->insertEffect (
                Objects::CEffect::fromJSON (*cur, object, container)
            );
        }
    }

    if (dependencies_it != data.end () && (*dependencies_it).is_array () == true)
    {
        auto cur = (*dependencies_it).begin ();
        auto end = (*dependencies_it).end ();

        for (; cur != end; cur ++)
        {
            object->insertDependency (*cur);
        }
    }

    return object;
}

const glm::vec3& CObject::getOrigin () const
{
    return this->m_origin;
}

const glm::vec3& CObject::getScale () const
{
    return this->m_scale;
}

const glm::vec3& CObject::getAngles () const
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

const std::vector<uint32_t>& CObject::getDependencies () const
{
    return this->m_dependencies;
}

const bool CObject::isVisible () const
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
void CObject::insertDependency (uint32_t dependency)
{
    this->m_dependencies.push_back (dependency);
}