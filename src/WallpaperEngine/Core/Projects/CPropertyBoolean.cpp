#include <sstream>

#include "CPropertyBoolean.h"
#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Projects;

CPropertyBoolean* CPropertyBoolean::fromJSON (json data, const std::string& name)
{
    json::const_iterator value = data.find ("value");
    std::string text = jsonFindDefault <std::string> (data, "text", "");

    return new CPropertyBoolean (
        *value,
        name,
        text
    );
}

bool CPropertyBoolean::getValue ()
{
    return this->m_value;
}

void CPropertyBoolean::update (const std::string& value)
{
    this->m_value = value == "1" || value == "true";
}

std::string CPropertyBoolean::dump () const
{
    std::stringstream ss;

    ss
        << this->m_name << " - boolean" << std::endl
        << "\t" << "Description: " << this->m_text << std::endl
        << "\t" << "Value: " << this->m_value;

    return ss.str();
}

CPropertyBoolean::CPropertyBoolean (bool value, const std::string& name, const std::string& text) :
    CProperty (name, Type, text),
    m_value (value)
{
}

const std::string CPropertyBoolean::Type = "bool";