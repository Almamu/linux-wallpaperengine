#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    class CAlphaRandom : CInitializer
    {
    public:
        const double getMinimum () const;
        const double getMaximum () const;
    protected:
        friend class CInitializer;

        static CAlphaRandom* fromJSON (json data, uint32_t id);

        CAlphaRandom (uint32_t id, double min, double max);
    private:
        double m_max;
        double m_min;
    };
};
