#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include "WallpaperEngine/Core/Core.h"

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    class CVelocityRandom : CInitializer
    {
    public:
        const irr::core::vector3df& getMinimum () const;
        const irr::core::vector3df& getMaximum () const;
    protected:
        friend class CInitializer;

        static CVelocityRandom* fromJSON (json data, irr::u32 id);

        CVelocityRandom (irr::u32 id, irr::core::vector3df min, irr::core::vector3df max);
    private:
        irr::core::vector3df m_max;
        irr::core::vector3df m_min;
    };
};
