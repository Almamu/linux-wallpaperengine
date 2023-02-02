#include "common.h"
#include <iostream>
#include "CProperty.h"
#include "CPropertyColor.h"
#include "CPropertyCombo.h"
#include "CPropertySlider.h"
#include "CPropertyBoolean.h"
#include "CPropertyText.h"

using namespace WallpaperEngine::Core::Projects;

CProperty* CProperty::fromJSON (json data, const std::string& name)
{
    auto type = jsonFindRequired (data, "type", "Project properties must have the type field");

    if (*type == CPropertyColor::Type)
        return CPropertyColor::fromJSON (data, name);
    if (*type == CPropertyBoolean::Type)
        return CPropertyBoolean::fromJSON (data, name);
    if (*type == CPropertySlider::Type)
        return CPropertySlider::fromJSON (data, name);
    if (*type == CPropertyCombo::Type)
        return CPropertyCombo::fromJSON (data, name);
    if (*type == CPropertyText::Type)
        return CPropertyText::fromJSON (data, name);


    // show the error and ignore this property
    sLog.error ("Unexpected type for property: ", *type);
    sLog.error (data);

    return nullptr;
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