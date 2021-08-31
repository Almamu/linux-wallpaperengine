#pragma once

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles
{
    using json = nlohmann::json;

    class CEmitter
    {
    public:
        static CEmitter* fromJSON (json data);

        const std::string& getName () const;
        const uint32_t getDistanceMax () const;
        const uint32_t getDistanceMin () const;
        const glm::vec3& getDirections () const;
        const glm::vec3& getOrigin () const;
        const double getRate () const;
    protected:
        CEmitter (
            const glm::vec3& directions,
            uint32_t distancemax,
            uint32_t distancemin,
            uint32_t id,
            std::string name,
            const glm::vec3& origin,
            double rate
        );
    private:
        glm::vec3 m_directions;
        uint32_t m_distancemax;
        uint32_t m_distancemin;
        uint32_t m_id;
        std::string m_name;
        glm::vec3 m_origin;
        double m_rate;
    };
};
