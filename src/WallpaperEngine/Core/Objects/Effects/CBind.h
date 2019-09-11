#pragma once

#include <string>

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

namespace WallpaperEngine::Core::Objects::Effects
{
    using json = nlohmann::json;

    class CBind
    {
    public:
        static CBind* fromJSON (json data);

        CBind (std::string name, irr::u32 index);

        const std::string& getName () const;
        const irr::u32& getIndex () const;

    private:
        std::string m_name;
        irr::u32 m_index;
    };
}
