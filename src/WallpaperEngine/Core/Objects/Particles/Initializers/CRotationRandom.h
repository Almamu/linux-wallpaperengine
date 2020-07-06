#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    class CRotationRandom : CInitializer
    {
    public:
        irr::core::vector3df getMinimum ();
        irr::core::vector3df getMaximum ();
    protected:
        friend class CInitializer;

        static CRotationRandom* fromJSON (json data, irr::u32 id);

        CRotationRandom (irr::u32 id, irr::core::vector3df min, irr::core::vector3df max);
    private:
        irr::core::vector3df m_max;
        irr::core::vector3df m_min;
    };
};
