#include "CProperty.h"

#include <utility>
#include "CPropertyColor.h"

namespace WallpaperEngine::Core::Projects
{
    CProperty* CProperty::fromJSON (json data, const std::string& name)
    {
        json::const_iterator type = data.find ("type");
        json::const_iterator value = data.find ("value");
        json::const_iterator text = data.find ("text");

        if (value == data.end ())
        {
            throw std::runtime_error ("Project properties must have the value field");
        }

        if (type == data.end ())
        {
            throw std::runtime_error ("Project properties must have the type field");
        }

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

    std::string& CProperty::getName ()
    {
        return this->m_name;
    }

    std::string& CProperty::getType ()
    {
        return this->m_type;
    }

    std::string& CProperty::getText ()
    {
        return this->m_text;
    }
};
