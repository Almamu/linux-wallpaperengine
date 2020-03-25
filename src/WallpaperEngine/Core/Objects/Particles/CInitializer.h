#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles
{
    using json = nlohmann::json;

    class CInitializer
    {
    public:
        static CInitializer* fromJSON (json data);

        const std::string& getName () const;
        const irr::u32 getId () const;
    protected:
        CInitializer (irr::u32 id, std::string name);
    private:
        irr::u32 m_id;
        std::string m_name;
    };
};
