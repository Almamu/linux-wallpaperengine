#pragma once

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Particles
{
    using json = nlohmann::json;

    class CInitializer
    {
    public:
        static CInitializer* fromJSON (json data);

        const std::string& getName () const;
        const uint32_t getId () const;
    protected:
        CInitializer (uint32_t id, std::string name);
    private:
        uint32_t m_id;
        std::string m_name;
    };
};
