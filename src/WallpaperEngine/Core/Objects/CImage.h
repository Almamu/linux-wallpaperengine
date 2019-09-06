#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"

#include "WallpaperEngine/Core/CObject.h"

namespace WallpaperEngine::Core::Objects
{
    using json = nlohmann::json;

    class CImage : public CObject
    {
        friend class CObject;

    public:
        static CObject* fromJSON (
                json data,
                bool visible,
                irr::u32 id,
                std::string name,
                const irr::core::vector3df& origin,
                const irr::core::vector3df& scale,
                const irr::core::vector3df& angles
        );

        Images::CMaterial* getMaterial ();

    protected:
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

        static const std::string Type;

    private:
        irr::core::vector2df m_size;
        Images::CMaterial* m_material;
    };
};
