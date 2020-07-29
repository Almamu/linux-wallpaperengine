#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    class CRotationRandom : CInitializer
    {
    public:
        const irr::core::vector3df getMinimumVector () const;
        const irr::core::vector3df getMaximumVector () const;
        const irr::f64             getMinimumNumber () const;
        const irr::f64             getMaximumNumber () const;

        const bool isMinimumVector () const;
        const bool isMinimumNumber () const;
        const bool isMaximumVector () const;
        const bool isMaximumNumber () const;

    protected:
        friend class CInitializer;

        static CRotationRandom* fromJSON (json data, irr::u32 id);

        CRotationRandom (
            irr::u32 id,
            irr::core::vector3df minVector,
            irr::f64 minNumber,
            bool isMinimumVector,
            irr::core::vector3df maxVector,
            irr::f64 maxNumber,
            bool isMaximumVector
        );
    private:
        irr::core::vector3df m_maxVector;
        irr::f64             m_maxNumber;
        irr::core::vector3df m_minVector;
        irr::f64             m_minNumber;

        bool m_isMinimumVector;
        bool m_isMaximumVector;
    };
};
