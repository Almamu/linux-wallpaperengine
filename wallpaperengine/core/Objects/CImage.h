#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

#include "wallpaperengine/core/CObject.h"
#include "Images/CMaterial.h"

namespace wp::core::Objects
{
    using json = nlohmann::json;

    class CImage : CObject
    {
    protected:
        friend class CObject;

        static CObject* fromJSON (
            json data,
            bool visible,
            irr::u32 id,
            std::string name,
            const irr::core::vector3df& origin,
            const irr::core::vector3df& scale,
            const irr::core::vector3df& angles
        );

        CImage (
                Images::CMaterial* material,
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
