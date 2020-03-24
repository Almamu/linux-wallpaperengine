#include "CProperty.h"
#include "CPropertyColor.h"

#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Projects;

CProperty* CProperty::fromJSON (json data, const std::string& name)
{
    auto type = jsonFindValueRequired(&data, "type", "Project properties must have the type field");
    auto value = jsonFindValueRequired(&data, "value", "Project properties must have the value field");
    auto text = data.find ("text");

    if (*type == CPropertyColor::Type)
    {
        return CPropertyColor::fromJSON (data, name);
    }

    throw std::runtime_error ("Unexpected type for property");
}

CProperty::CProperty (std::string name, std::string type, std::string text) :
    m_name (std::move(name)),
    m_type (std::move(type)),
    m_text (std::move(text))
{
}

const std::string& CProperty::getName () const
{
    return this->m_name;
}

const std::string& CProperty::getType () const
{
    return this->m_type;
}

const std::string& CProperty::getText () const
{
    return this->m_text;
}