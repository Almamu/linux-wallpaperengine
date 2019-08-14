#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace wp::core::objects::particles
{
    using json = nlohmann::json;

    class emitter
    {
    public:
        const std::string& getName () const;
        irr::u32 getDistanceMax () const;
        irr::u32 getDistanceMin () const;
        irr::core::vector3df* getDirections ();
        irr::core::vector3df* getOrigin ();
        irr::f64 getRate () const;
    protected:
        friend class particle;

        static emitter* fromJSON (json data);

        emitter (
            const irr::core::vector3df& directions,
            irr::u32 distancemax,
            irr::u32 distancemin,
            irr::u32 id,
            std::string name,
            const irr::core::vector3df& origin,
            irr::f64 rate
        );
    private:
        irr::core::vector3df m_directions;
        irr::u32 m_distancemax;
        irr::u32 m_distancemin;
        irr::u32 m_id;
        std::string m_name;
        irr::core::vector3df m_origin;
        irr::f64 m_rate;
    };
};
