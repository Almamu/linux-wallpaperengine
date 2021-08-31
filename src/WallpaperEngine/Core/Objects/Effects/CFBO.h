#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace WallpaperEngine::Core::Objects::Effects
{
    using json = nlohmann::json;

    class CFBO
    {
    public:
        CFBO (std::string name, float scale, std::string format);

        static CFBO* fromJSON (json data);

        const std::string& getName () const;
        const float& getScale () const;
        const std::string& getFormat () const;

    private:
        std::string m_name;
        float m_scale;
        std::string m_format;
    };
}