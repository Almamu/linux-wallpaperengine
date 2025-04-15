#include <sstream>
#include <utility>

#include "CPropertyCombo.h"

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Core::Projects;

const CPropertyCombo* CPropertyCombo::fromJSON (const json& data, std::string name) {
    std::vector<const CPropertyComboValue*> values;
    const auto options = jsonFindRequired (data, "options", "Options for a property combo is required");

    if (!options->is_array ())
        sLog.exception ("Property combo options should be an array");

    for (auto& cur : (*options)) {
        // TODO: PROPERLY REPORT THESE ISSUES
        if (!cur.is_object ())
            continue;

        // check for label and value to ensure they're there
        auto prop = new CPropertyComboValue {
            .label = jsonFindRequired <std::string> (cur, "label", "Label is required for a property combo option"),
            .value = jsonFindRequired <std::string> (cur, "value", "Value is required for a property combo option")
        };

        values.push_back (prop);
    }

    return new CPropertyCombo (
        std::move(name),
        jsonFindDefault<std::string> (data, "text", ""),
        jsonFindRequired (data, "value", "Value is required for a property combo")->dump (),
        values
    );
}

CPropertyCombo::CPropertyCombo (
    std::string name, std::string text, std::string defaultValue, std::vector<const CPropertyComboValue*> values
) :
    CProperty (std::move(name), Type, std::move(text)),
    m_defaultValue (std::move(defaultValue)),
    m_values (std::move(values)) {}

CPropertyCombo::~CPropertyCombo () {
    for (const auto* value : this->m_values)
        delete value;
}

const std::string& CPropertyCombo::getValue () const {
    return this->m_defaultValue;
}

std::string CPropertyCombo::dump () const {
    std::stringstream ss;

    ss << this->m_name << " - combolist" << std::endl
       << "\t"
       << "Description: " << this->m_text << std::endl
       << "\t"
       << "Value: " << this->m_defaultValue << std::endl
       << "\t\t"
       << "Posible values:" << std::endl;

    for (const auto cur : this->m_values)
        ss << "\t\t" << cur->label << " -> " << cur->value << std::endl;

    return ss.str ();
}

void CPropertyCombo::update (const std::string& value) const {
    bool found = false;

    // ensure the value is present somewhere in the value list
    for (const auto cur : this->m_values) {
        if (cur->value != value)
            continue;

        found = true;
    }

    if (!found)
        sLog.exception ("Assigning invalid value to property ", this->m_name);

    this->m_defaultValue = value;
}

const std::string CPropertyCombo::Type = "combo";