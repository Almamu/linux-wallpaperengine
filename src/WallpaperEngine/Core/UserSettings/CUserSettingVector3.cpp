#include "CUserSettingVector3.h"

#include <utility>
#include "WallpaperEngine/Core/Core.h"

#include "WallpaperEngine/Core/Projects/CProperty.h"
#include "WallpaperEngine/Core/Projects/CPropertyColor.h"
#include "WallpaperEngine/Core/Projects/CPropertySlider.h"
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Projects;
using namespace WallpaperEngine::Core::UserSettings;

CUserSettingVector3::CUserSettingVector3 (
    bool hasCondition, glm::vec3 defaultValue, const Projects::CProperty* source, std::string expectedValue
) :
    CUserSettingValue (Type),
    m_hasCondition (hasCondition),
    m_source (source),
    m_expectedValue (std::move(expectedValue)) {
    this->update (defaultValue);

    if (this->m_source != nullptr) {
        this->m_source->subscribe ([this](const Projects::CProperty* property) -> void {
            if (this->m_hasCondition) {
                sLog.error ("Don't know how to check for condition on a float property... Expected value: ", this->m_expectedValue);
                return;
            }

            this->update (property->getVec3 ());
        });
    }
}

const CUserSettingVector3* CUserSettingVector3::fromJSON (const nlohmann::json& data, const CProject& project) {
    bool hasCondition = false;
    const Projects::CProperty* sourceProperty = nullptr;
    glm::vec3 defaultValue;
    std::string source;
    std::string expectedValue;

    if (data.is_object ()) {
        auto userIt = data.find ("user");
        defaultValue = jsonFindDefault (data, "value", glm::vec3()); // is this default value right?

        if (userIt != data.end ()) {
            if (userIt->is_string ()) {
                source = *userIt;
            } else {
                hasCondition = true;
                source = jsonFindRequired <std::string> (userIt, "name", "Name for conditional setting must be present");
                expectedValue =
                    jsonFindRequired <std::string> (userIt, "condition", "Condition for conditional setting must be present");
            }

            for (const auto& [key, property] : project.getProperties ()) {
                if (key == source) {
                    sourceProperty = property;
                    break;
                }
            }

            if (sourceProperty == nullptr) {
                sLog.error ("Cannot find property ", source, " to get value from for user setting value, using default value: (", defaultValue.x, ",", defaultValue.y, ",", defaultValue.z, ")");
            }
        } else {
            sLog.error ("Vector property doesn't have user member, this could mean an scripted value");
        }
    } else {
        if (!data.is_string ())
            sLog.exception ("Expected vector value on user settings");

        defaultValue = WallpaperEngine::Core::aToColorf (data.get<std::string> ().c_str ());
    }

    return new CUserSettingVector3 (hasCondition, defaultValue, sourceProperty, expectedValue);
}

const CUserSettingVector3* CUserSettingVector3::fromScalar (const glm::vec3 value) {
    return new CUserSettingVector3 (false, value, nullptr, "");
}

std::string CUserSettingVector3::Type = "color";