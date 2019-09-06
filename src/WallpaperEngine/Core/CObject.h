#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/Objects/CEffect.h"

namespace WallpaperEngine::Core::Objects
{
    class CEffect;
}

namespace WallpaperEngine::Core
{
    using json = nlohmann::json;

    class CObject
    {
    public:
        static CObject* fromJSON (json data);

        template<class T> const T* As () const { assert (Is<T> ()); return (const T*) this; }
        template<class T> T* As () { assert (Is<T> ()); return (T*) this; }

        template<class T> bool Is () { return this->m_type == T::Type; }

        std::vector<Objects::CEffect*>* getEffects ();
        int getId ();

        irr::core::vector3df* getOrigin ();
        irr::core::vector3df* getScale ();
        irr::core::vector3df* getAngles ();

    protected:
        CObject (
            bool visible,
            irr::u32 id,
            std::string name,
            std::string type,
            const irr::core::vector3df& origin,
            const irr::core::vector3df& scale,
            const irr::core::vector3df& angles
        );

        void insertEffect (Objects::CEffect* effect);
    private:
        std::string m_type;

        bool m_visible;
        irr::u32 m_id;
        std::string m_name;
        irr::core::vector3df m_origin;
        irr::core::vector3df m_scale;
        irr::core::vector3df m_angles;

        std::vector<Objects::CEffect*> m_effects;
    };
};
