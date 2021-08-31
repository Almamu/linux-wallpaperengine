#pragma once

#include "WallpaperEngine/Core/Objects/Particles/CInitializer.h"

#include <nlohmann/json.hpp>

namespace WallpaperEngine::Core::Objects::Particles::Initializers
{
    class CRotationRandom : CInitializer
    {
    public:
        const glm::vec3 getMinimumVector () const;
        const glm::vec3 getMaximumVector () const;
        const double             getMinimumNumber () const;
        const double             getMaximumNumber () const;

        const bool isMinimumVector () const;
        const bool isMinimumNumber () const;
        const bool isMaximumVector () const;
        const bool isMaximumNumber () const;

    protected:
        friend class CInitializer;

        static CRotationRandom* fromJSON (json data, uint32_t id);

        CRotationRandom (
            uint32_t id,
            glm::vec3 minVector,
            double minNumber,
            bool isMinimumVector,
            glm::vec3 maxVector,
            double maxNumber,
            bool isMaximumVector
        );
    private:
        glm::vec3 m_maxVector;
        double             m_maxNumber;
        glm::vec3 m_minVector;
        double             m_minNumber;

        bool m_isMinimumVector;
        bool m_isMaximumVector;
    };
};
