#pragma once

#include <irrlicht/irrlicht.h>

#include "CProperty.h"

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Projects
{
    using json = nlohmann::json;

    class CPropertyColor : public CProperty
    {
    public:
        static CPropertyColor* fromJSON (json data, const std::string& name);

        const irr::video::SColor& getValue () const;

        static const std::string Type;

    private:
        CPropertyColor (irr::video::SColor color, const std::string& name, const std::string& text);

        irr::video::SColor m_color;
    };
};
