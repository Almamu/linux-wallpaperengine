#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace wp::core
{
    using json = nlohmann::json;

    class object
    {
    public:
        static object* fromJSON (json data);
    protected:
        object (
            bool visible,
            irr::u32 id,
            std::string name,
            const irr::core::vector3df& origin,
            const irr::core::vector3df& scale,
            const irr::core::vector3df& angles
        );
    private:
        bool m_visible;
        irr::u32 m_id;
        std::string m_name;
        irr::core::vector3df m_origin;
        irr::core::vector3df m_scale;
        irr::core::vector3df m_angles;
    };
};
