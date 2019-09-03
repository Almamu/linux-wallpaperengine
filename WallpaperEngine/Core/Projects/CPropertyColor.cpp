#include "CPropertyColor.h"
#include "../Core.h"

namespace WallpaperEngine::Core::Projects
{
    CPropertyColor* CPropertyColor::fromJSON (json data, const std::string& name)
    {
        json::const_iterator value = data.find ("value");
        json::const_iterator text = data.find ("type");

        return new CPropertyColor (
            WallpaperEngine::Core::atoSColor (*value),
            name,
            *text
        );
    }

    irr::video::SColor* CPropertyColor::getValue ()
    {
        return &this->m_color;
    }

    CPropertyColor::CPropertyColor (irr::video::SColor color, const std::string& name, const std::string& text) :
        CProperty (name, Type, text),
        m_color (color)
    {
    }

    const std::string CPropertyColor::Type = "color";
};