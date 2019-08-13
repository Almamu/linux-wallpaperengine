#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

#include "../object.h"
#include "images/material.h"

namespace wp::core::objects
{
    using json = nlohmann::json;

    class image : object
    {
    protected:
        friend class object;

        static object* fromJSON (
            json data,
            bool visible,
            irr::u32 id,
            std::string name,
            const irr::core::vector3df& origin,
            const irr::core::vector3df& scale,
            const irr::core::vector3df& angles
        );

        image (
            images::material* material,
            bool visible,
            irr::u32 id,
            std::string name,
            const irr::core::vector3df& origin,
            const irr::core::vector3df& scale,
            const irr::core::vector3df& angles,
            const irr::core::vector2df& size
        );

    private:
        irr::core::vector2df m_size;
    };
};
