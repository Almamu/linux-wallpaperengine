#include "CUserSettingFloat.h"

#include <utility>
#include "WallpaperEngine/Core/Core.h"

#include "WallpaperEngine/Core/CProject.h"
#include "WallpaperEngine/Core/Projects/CProperty.h"
#include "WallpaperEngine/Core/Projects/CPropertySlider.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Projects;
using namespace WallpaperEngine::Core::UserSettings;

CUserSettingFloat::CUserSettingFloat (
    bool hasCondition, float defaultValue, std::shared_ptr <const Projects::CProperty> source, std::string expectedValue
) :
    CUserSettingValue (),
    m_default (defaultValue),
    m_hasCondition (hasCondition),
    m_source (source),
    m_expectedValue (std::move(expectedValue)) {
    this->update (defaultValue);

    if (this->m_source != nullptr) {
        this->m_source->subscribe ([this](const Projects::CProperty* property) -> void {
            if (!this->m_hasCondition) {
                this->update (property->getFloat ());
            } else {
                sLog.error ("Don't know how to check for condition on a float property... Expected value: ", this->m_expectedValue);
            }
        });
    }
}

const CUserSettingFloat* CUserSettingFloat::fromJSON (const nlohmann::json& data, const std::map <std::string, std::shared_ptr <Projects::CProperty>>& properties) {
    float defaultValue;
    std::string source;
    std::string expectedValue;
    bool hasCondition = false;
    std::shared_ptr <const Projects::CProperty> sourceProperty = nullptr;

    if (data.is_object ()) {
        auto animation = data.find ("animation");
        auto userIt = data.find ("user");
        defaultValue = jsonFindDefault (data, "value", 1.0f); // is this default value right?

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

        if (animation != data.end ()) {
            sLog.error ("Detected a setting with animation data, which is not supported yet!");
        }
        } else {
            sLog.error ("Float property doesn't have user member, this could mean an scripted value");
        }
    } else {
        if (!data.is_number ())
            sLog.exception ("Expected numeric value on user settings");

        defaultValue = data.get<float> ();
    }

    return new CUserSettingFloat (hasCondition, defaultValue, sourceProperty, expectedValue);
}

const CUserSettingFloat* CUserSettingFloat::fromScalar (const float value) {
    return new CUserSettingFloat (false, value, nullptr, "");
}
