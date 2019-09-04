#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/Objects/CEffect.h"

namespace WallpaperEngine::Core
{
    using json = nlohmann::json;

    class CObject
    {
    public:
        static CObject* fromJSON (json data);

        std::vector<Objects::CEffect*>* getEffects ();
    protected:
        CObject (
            bool visible,
            irr::u32 id,
            std::string name,
            const irr::core::vector3df& origin,
            const irr::core::vector3df& scale,
            const irr::core::vector3df& angles
        );

        void insertEffect (Objects::CEffect* effect);
    private:
        bool m_visible;
        irr::u32 m_id;
        std::string m_name;
        irr::core::vector3df m_origin;
        irr::core::vector3df m_scale;
        irr::core::vector3df m_angles;

        std::vector<Objects::CEffect*> m_effects;
    };
};
