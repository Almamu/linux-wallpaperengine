#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "wallpaperengine/core/CObject.h"

namespace wp::core::Objects
{
    using json = nlohmann::json;

    class CSound : CObject
    {
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
        std::vector<std::string>* getSounds ();
    protected:

        CSound (
            bool visible,
            irr::u32 id,
            std::string name,
            const irr::core::vector3df& origin,
            const irr::core::vector3df& scale,
            const irr::core::vector3df& angles
        );
    private:
        std::vector<std::string> m_sounds;
    };
}
