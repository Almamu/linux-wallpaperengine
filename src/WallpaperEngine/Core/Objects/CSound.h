#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "WallpaperEngine/Core/CObject.h"

namespace WallpaperEngine::Core::Objects
{
    using json = nlohmann::json;

    class CSound : public CObject
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

        void insertSound (std::string filename);
        const std::vector<std::string>& getSounds () const;

    protected:
        CSound (
            bool visible,
            irr::u32 id,
            std::string name,
            const irr::core::vector3df& origin,
            const irr::core::vector3df& scale,
            const irr::core::vector3df& angles
        );

        static const std::string Type;
    private:
        std::vector<std::string> m_sounds;
    };
}
