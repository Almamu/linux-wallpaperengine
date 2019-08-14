#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace wp::core::Objects::Particles
{
    using json = nlohmann::json;

    class CControlPoint
    {
    public:
        irr::core::vector3df* getOffset ();
        irr::u32 getFlags ();
    protected:
        friend class CParticle;

        static CControlPoint* fromJSON (json data);

        CControlPoint (irr::u32 id, irr::u32 flags = 0);

        void setOffset (const irr::core::vector3df& offset);
        void setFlags (irr::u32 flags);
    private:
        irr::u32 m_id;
        irr::u32 m_flags;
        irr::core::vector3df m_offset;
    };
};
