#include <sstream>

#include "CPropertyColor.h"

using namespace WallpaperEngine::Core::Projects;

CPropertyColor* CPropertyColor::fromJSON (json data, const std::string& name)
{
    std::string value = *jsonFindRequired (data, "value", "Color property must have a value");
    std::string text = jsonFindDefault <std::string> (data, "text", "");
    FloatColor color (0, 0, 0, 0);

    if (value.find ('.') == std::string::npos && value != "0 0 0" && value != "1 1 1")
    {
        IntegerColor intcolor = WallpaperEngine::Core::aToColori (value);

        color.r = intcolor.r / 255.0;
        color.g = intcolor.g / 255.0;
        color.b = intcolor.b / 255.0;
        color.a = intcolor.a / 255.0;
    }
    else
    {
        color = WallpaperEngine::Core::aToColorf (value);
    }

    return new CPropertyColor (
        color,
        name,
        text
    );
}

const FloatColor& CPropertyColor::getValue () const
{
    return this->m_color;
}

std::string CPropertyColor::dump () const
{
    std::stringstream ss;

    ss
        << this->m_name << " - color" << std::endl
        << "\t" << "Description: " << this->m_text << std::endl
        << "\t"
        << "R: " << this->m_color.r
        << " G: " << this->m_color.g
        << " B: " << this->m_color.b
        << " A: " << this->m_color.a;

    return ss.str();
}

CPropertyColor::CPropertyColor (FloatColor color, const std::string& name, const std::string& text) :
    CProperty (name, Type, text),
    m_color (color)
{
}

const std::string CPropertyColor::Type = "color";