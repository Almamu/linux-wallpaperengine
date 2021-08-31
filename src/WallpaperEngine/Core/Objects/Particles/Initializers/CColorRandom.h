#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    using namespace WallpaperEngine::Core::Types;

    class CColorRandom : CInitializer
    {
    public:
        const IntegerColor& getMinimum () const;
        const IntegerColor& getMaximum () const;
    protected:
        friend class CInitializer;

        static CColorRandom* fromJSON (json data, uint32_t id);

        CColorRandom (uint32_t id, IntegerColor min, IntegerColor max);
    private:
        IntegerColor m_max;
        IntegerColor m_min;
    };
};
