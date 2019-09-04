#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Objects::Particles
{
    using json = nlohmann::json;

    class CInitializer
    {
    public:
        std::string& getName ();
        irr::u32 getId ();
    protected:
        friend class CParticle;

        static CInitializer* fromJSON (json data);

        CInitializer (irr::u32 id, std::string name);
    private:
        irr::u32 m_id;
        std::string m_name;
    };
};
