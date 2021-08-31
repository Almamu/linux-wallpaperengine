#pragma once

#include <string>

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Objects::Effects
{
    using json = nlohmann::json;

    class CBind
    {
    public:
        static CBind* fromJSON (json data);

        CBind (std::string name, uint32_t index);

        const std::string& getName () const;
        const uint32_t& getIndex () const;

    private:
        std::string m_name;
        uint32_t m_index;
    };
}
