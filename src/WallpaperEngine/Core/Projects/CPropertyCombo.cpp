#include <sstream>
#include <utility>

#include "CPropertyCombo.h"

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Core::Projects;

std::shared_ptr<CPropertyCombo> CPropertyCombo::fromJSON (const json& data, std::string name) {
    std::vector<CPropertyComboValue> values;
    const auto options = jsonFindRequired (data, "options", "Options for a property combo is required");

    if (!options->is_array ())
        sLog.exception ("Property combo options should be an array");

    for (auto& cur : (*options)) {
        // TODO: PROPERLY REPORT THESE ISSUES
        if (!cur.is_object ())
            continue;

        // check for label and value to ensure they're there
        values.push_back ({
            .label = jsonFindRequired<std::string> (cur, "label", "Label is required for a property combo option"),
            .value = jsonFindRequired<std::string> (cur, "value", "Value is required for a property combo option")
        });
    }

    return std::make_shared <CPropertyCombo> (
        std::move(name),
        jsonFindDefault<std::string> (data, "text", ""),
        jsonFindRequired<std::string> (data, "value", "Value is required for a property combo"),
        values
    );
}

CPropertyCombo::CPropertyCombo (
    std::string name, std::string text, const std::string& defaultValue,
    std::vector<CPropertyComboValue> values
) :
    CProperty (std::move(name), std::move(text)),
    m_values (std::move(values)) {
    this->set (defaultValue);
}

std::string CPropertyCombo::dump () const {
    std::stringstream ss;

    ss << this->m_name << " - combolist" << std::endl
       << "\t"
       << "Description: " << this->m_text << std::endl
       << "\t"
       << "Value: " << &this->getInt () << std::endl
       << "\t\t"
       << "Posible values:" << std::endl;

    for (const auto& cur : this->m_values)
        ss << "\t\t" << cur.label << " -> " << cur.value << std::endl;

    return ss.str ();
}

void CPropertyCombo::set (const std::string& value) {
    bool found = false;
    int index = 0;

    // ensure the value is present somewhere in the value list
    for (const auto& cur : this->m_values) {
        if (cur.value == value) {
            found = true;
            break;
        }

        index ++;
    }

    if (!found)
        sLog.exception ("Assigning invalid value to property ", this->m_name);

    this->update (index);
}

int CPropertyCombo::translateValueToIndex (const std::string& value) const {
    bool found = false;
    int index = 0;

    // ensure the value is present somewhere in the value list
    for (const auto& cur : this->m_values) {
        if (cur.value == value) {
            found = true;
            break;
        }

        index ++;
    }

    if (!found) {
        return -1;
    }

    return index;
}

const char* CPropertyCombo::getType () const {
    return "combo";
}
