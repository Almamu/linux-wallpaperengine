#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/CObject.h"
#include "CSound.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Objects;

CSound::CSound (
        bool visible,
        irr::u32 id,
        std::string name,
        const irr::core::vector3df& origin,
        const irr::core::vector3df& scale,
        const irr::core::vector3df& angles) :
        CObject (visible, id, std::move(name), Type, origin, scale, angles)
{
}

WallpaperEngine::Core::CObject* CSound::fromJSON (
        json data,
        bool visible,
        irr::u32 id,
        std::string name,
        const irr::core::vector3df& origin,
        const irr::core::vector3df& scale,
        const irr::core::vector3df& angles)
{
    auto sound_it = jsonFindValueRequired(&data, "sound", "Sound information not present");

    if ((*sound_it).is_array () == false)
    {
        throw std::runtime_error ("Expected sound list");
    }

    CSound* sound = new CSound (
        visible,
        id,
        name,
        origin,
        scale,
        angles
    );

    auto cur = (*sound_it).begin ();
    auto end = (*sound_it).end ();

    for (; cur != end; cur ++)
    {
        sound->insertSound (*cur);
    }

    return sound;
}

void CSound::insertSound (std::string filename)
{
    this->m_sounds.push_back (filename);
}

const std::vector<std::string>& CSound::getSounds () const
{
    return this->m_sounds;
}

const std::string CSound::Type = "sound";