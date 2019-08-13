#include <irrlicht/irrlicht.h>

#include "../object.h"
#include "sound.h"

using namespace wp::core::objects;
sound::sound (
        bool visible,
        irr::u32 id,
        std::string name,
        const irr::core::vector3df& origin,
        const irr::core::vector3df& scale,
        const irr::core::vector3df& angles) :
    object (visible, id, std::move(name), origin, scale, angles)
{
}

wp::core::object* sound::fromJSON (
        json data,
        bool visible,
        irr::u32 id,
        std::string name,
        const irr::core::vector3df& origin,
        const irr::core::vector3df& scale,
        const irr::core::vector3df& angles)
{
    json::const_iterator sound_it = data.find ("sound");

    if (sound_it == data.end ())
    {
        throw std::runtime_error ("Sound information not present");
    }

    if ((*sound_it).is_array () == false)
    {
        throw std::runtime_error ("Expected sound list");
    }

    sound* sound = new class sound (
        visible,
        id,
        name,
        origin,
        scale,
        angles
    );

    json::const_iterator cur = (*sound_it).begin ();
    json::const_iterator end = (*sound_it).end ();

    for (; cur != end; cur ++)
    {
        sound->insertSound (*cur);
    }

    return sound;
}

void sound::insertSound (std::string filename)
{
    this->m_sounds.push_back (filename);
}

std::vector<std::string>* sound::getSounds ()
{
    return &this->m_sounds;
}