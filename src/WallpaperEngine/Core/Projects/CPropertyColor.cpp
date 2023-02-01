#include <sstream>

#include "CPropertyColor.h"

using namespace WallpaperEngine::Core::Projects;

glm::vec3 ParseColor (std::string value)
{
    if (value.find (',') != std::string::npos)
    {
        // replace commas with dots so it can be parsed
        std::replace (value.begin (), value.end (), ',', ' ');
    }

    if (value.find ('.') == std::string::npos && value != "0 0 0" && value != "1 1 1")
    {
        glm::ivec3 intcolor = WallpaperEngine::Core::aToColori (value);

        return {
            intcolor.r / 255.0,
            intcolor.g / 255.0,
            intcolor.b / 255.0
        };
    }

    return WallpaperEngine::Core::aToColorf (value);
}

CPropertyColor* CPropertyColor::fromJSON (json data, const std::string& name)
{
    std::string value = *jsonFindRequired (data, "value", "Color property must have a value");
    std::string text = jsonFindDefault <std::string> (data, "text", "");

    return new CPropertyColor (
        ParseColor (value),
        name,
        text
    );
}

const glm::vec3& CPropertyColor::getValue () const
{
    return this->m_color;
}

void CPropertyColor::update (const std::string& value)
{
    this->m_color = ParseColor (std::string (value));
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
        << " B: " << this->m_color.b;

    return ss.str();
}

CPropertyColor::CPropertyColor (glm::vec3 color, const std::string& name, const std::string& text) :
    CProperty (name, Type, text),
    m_color (color)
{
}

const std::string CPropertyColor::Type = "color";