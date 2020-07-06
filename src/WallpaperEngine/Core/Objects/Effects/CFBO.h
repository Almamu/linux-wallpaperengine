#pragma once

#include <string>

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

namespace WallpaperEngine::Core::Objects::Effects
{
    using json = nlohmann::json;

    class CFBO
    {
    public:
        CFBO (std::string name, irr::f32 scale, std::string format);

        static CFBO* fromJSON (json data);

        const std::string& getName () const;
        const irr::f32& getScale () const;
        const std::string& getFormat () const;

    private:
        std::string m_name;
        irr::f32 m_scale;
        std::string m_format;
    };
}