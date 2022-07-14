#include "CPropertyCombo.h"
#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Projects;

CPropertyCombo* CPropertyCombo::fromJSON (json data, const std::string& name)
{
    auto value = data.find ("value");
    auto text = data.find ("type");
    auto options = jsonFindRequired (data, "options", "Options for a property combo is required");

    CPropertyCombo* combo = new CPropertyCombo (
        name,
        *text
    );

    if (options->is_array () == false)
        throw std::runtime_error ("Property combo options should be an array");

    auto cur = (*options).begin ();
    auto end = (*options).end ();

    for (; cur != end; cur ++)
    {
        if ((*cur).is_object () == false)
        {
            // TODO: PROPERLY REPORT THESE ISSUES
            continue;
        }

        // check for label and value to ensure they're there
        auto label = jsonFindRequired (*cur, "label", "Label is required for a property combo option");
        auto value = jsonFindRequired (*cur, "value", "Value is required for a property combo option");

        combo->addValue (*label, *value);
    }

    return combo;
}

CPropertyCombo::CPropertyCombo (const std::string& name, const std::string& text) :
    CProperty (name, Type, text)
{
}



void CPropertyCombo::addValue (std::string label, std::string value)
{
    CPropertyComboValue* prop = new CPropertyComboValue;

    prop->label = std::move (label);
    prop->value = std::move (value);

    this->m_values.push_back (prop);
}

const std::string CPropertyCombo::Type = "combo";