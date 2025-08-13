#include "CUserSettingBoolean.h"

#include <utility>
#include "WallpaperEngine/Core/Core.h"

#include "WallpaperEngine/Core/Projects/CProperty.h"
#include "WallpaperEngine/Core/Projects/CPropertyBoolean.h"
#include "WallpaperEngine/Core/Projects/CPropertyCombo.h"
#include "WallpaperEngine/Core/Projects/CPropertySlider.h"
#include "WallpaperEngine/Core/Projects/CPropertyText.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Projects;
using namespace WallpaperEngine::Core::UserSettings;

CUserSettingBoolean::CUserSettingBoolean (
    bool hasCondition, bool defaultValue, std::shared_ptr <const Projects::CProperty> source, std::string expectedValue
) :
    CUserSettingValue (),
    m_hasCondition (hasCondition),
    m_source (source),
    m_expectedValue (std::move(expectedValue)) {
    this->update (defaultValue);

    if (this->m_source != nullptr) {
        this->m_source->subscribe ([this](const Projects::CProperty* property) -> void {
            if (!this->m_hasCondition) {
                this->update (property->getBool ());
            } else if (property->is <CPropertyCombo> ()) {
                this->update (
                    property->as <CPropertyCombo> ()->translateValueToIndex (this->m_expectedValue) == property->getInt ()
                );
            } else {
                sLog.error ("Cannot update boolean user setting for an unknown property type ", property->getType ());
            }
        });
    }
}

const CUserSettingBoolean* CUserSettingBoolean::fromJSON (const nlohmann::json& data, const std::map <std::string, std::shared_ptr <Projects::CProperty>>& properties) {
    bool hasCondition = false;
    std::shared_ptr <const Projects::CProperty> sourceProperty = nullptr;
    bool defaultValue;
    std::string source;
    std::string expectedValue;

    if (data.is_object ()) {
        auto animation = data.find ("animation");
        auto userIt = data.find ("user");
        defaultValue = jsonFindDefault (data, "value", true); // is this default value right?

        if (userIt != data.end ()) {
            if (userIt->is_string ()) {
                source = *userIt;
            } else {
                hasCondition = true;
                source = jsonFindRequired <std::string> (userIt, "name", "Name for conditional setting must be present");
                expectedValue =
                    jsonFindRequired <std::string> (userIt, "condition", "Condition for conditional setting must be present");
            }

            const auto propertyIt = properties.find (source);

            if (propertyIt != properties.end ()) {
                sourceProperty = propertyIt->second;
            }

            if (sourceProperty == nullptr) {
                sLog.error ("Cannot find property ", source, " to get value from for user setting value, using default value: ", defaultValue);
            }
        } else {
            sLog.error ("Boolean property doesn't have user member, this could mean an scripted value");
        }

        if (animation != data.end ()) {
            sLog.error ("Detected a setting with animation data, which is not supported yet!");
        }
    } else {
        if (!data.is_boolean ())
            sLog.error ("Expected boolean value on user setting");

        defaultValue = data.get<bool> ();
    }

    return new CUserSettingBoolean (hasCondition, defaultValue, sourceProperty, expectedValue);
}

const CUserSettingBoolean* CUserSettingBoolean::fromScalar (const bool value) {
    return new CUserSettingBoolean (false, value, nullptr, "");
}
