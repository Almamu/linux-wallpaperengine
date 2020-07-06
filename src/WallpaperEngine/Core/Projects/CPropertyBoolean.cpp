#include "CPropertyBoolean.h"
#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Projects;

CPropertyBoolean* CPropertyBoolean::fromJSON (json data, const std::string& name)
{
    json::const_iterator value = data.find ("value");
    json::const_iterator text = data.find ("type");

    return new CPropertyBoolean (
        *value,
        name,
        *text
    );
}

bool CPropertyBoolean::getValue ()
{
    return &this->m_value;
}

CPropertyBoolean::CPropertyBoolean (bool value, const std::string& name, const std::string& text) :
    CProperty (name, Type, text),
    m_value (value)
{
}

const std::string CPropertyBoolean::Type = "bool";