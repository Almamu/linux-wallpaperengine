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
    bool hasCondition, bool hasSource, glm::vec3 defaultValue, std::string source, std::string expectedValue
) :
    CUserSettingValue (Type),
    m_default (defaultValue),
    m_hasCondition (hasCondition),
    m_hasSource (hasSource),
    m_source (std::move(source)),
    m_expectedValue (std::move(expectedValue)) {}

const CUserSettingVector3* CUserSettingVector3::fromJSON (const nlohmann::json& data) {
    bool hasCondition = false;
    bool hasSource = false;
    glm::vec3 defaultValue;
    std::string source;
    std::string expectedValue;

    if (data.is_object ()) {
        hasSource = true;
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
        } else {
            sLog.error ("Vector property doesn't have user member, this could mean an scripted value");
        }
    } else {
        if (!data.is_string ())
            sLog.exception ("Expected vector value on user settings");

        defaultValue = WallpaperEngine::Core::aToColorf (data.get<std::string> ().c_str ());
    }

    return new CUserSettingVector3 (hasCondition, hasSource, defaultValue, source, expectedValue);
}

const CUserSettingVector3* CUserSettingVector3::fromScalar (const glm::vec3 value) {
    return new CUserSettingVector3 (false, false, value, "", "");
}

const glm::vec3& CUserSettingVector3::getDefaultValue () const {
    return this->m_default;
}

const glm::vec3& CUserSettingVector3::processValue (const std::map<std::string, const Projects::CProperty*>& properties) const {
    if (!this->m_hasSource && !this->m_hasCondition)
        return this->getDefaultValue ();

    const auto property = properties.find (this->m_source);

    if (property != properties.end ()) {
        if (!this->m_hasCondition) {
            if (property->second->is<CPropertyColor> ())
                return property->second->as<CPropertyColor> ()->getValue ();
            if (property->second->is<CPropertySlider> ())
                return {property->second->as<CPropertySlider> ()->getValue (), property->second->as<CPropertySlider> ()->getValue (),
                        property->second->as<CPropertySlider> ()->getValue ()};

            sLog.exception ("Property without condition must match type (vector3)");
        }

        sLog.exception ("Vector property with condition doesn't match against combo value");
    }

    return this->m_default;
}

std::string CUserSettingVector3::Type = "color";