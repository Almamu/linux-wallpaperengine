#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    class CColorRandom : CInitializer
    {
    public:
        const glm::ivec3& getMinimum () const;
        const glm::ivec3& getMaximum () const;
    protected:
        friend class CInitializer;

        static CColorRandom* fromJSON (json data, uint32_t id);

        CColorRandom (uint32_t id, glm::ivec3 min, glm::ivec3 max);
    private:
        glm::ivec3 m_max;
        glm::ivec3 m_min;
    };
};
