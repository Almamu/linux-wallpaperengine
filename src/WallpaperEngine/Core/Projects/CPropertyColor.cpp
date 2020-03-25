#include "CPropertyColor.h"

using namespace WallpaperEngine::Core::Projects;

CPropertyColor* CPropertyColor::fromJSON (json data, const std::string& name)
{
    auto value = data.find ("value");
    auto text = data.find ("type");

    return new CPropertyColor (
        WallpaperEngine::Core::atoSColor (*value),
        name,
        *text
    );
}

const irr::video::SColor& CPropertyColor::getValue () const
{
    return this->m_color;
}

CPropertyColor::CPropertyColor (irr::video::SColor color, const std::string& name, const std::string& text) :
    CProperty (name, Type, text),
    m_color (color)
{
}

const std::string CPropertyColor::Type = "color";