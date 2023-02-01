#include "CUserSettingColor.h"
#include "WallpaperEngine/Core/Core.h"

#include "WallpaperEngine/Core/Projects/CProperty.h"
#include "WallpaperEngine/Core/Projects/CPropertyColor.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Projects;
using namespace WallpaperEngine::Core::UserSettings;

CUserSettingColor::CUserSettingColor (bool hasCondition, bool hasSource, glm::vec3 defaultValue, std::string source, std::string expectedValue) :
    CUserSettingValue (Type),
    m_hasCondition (hasCondition),
    m_hasSource(hasSource),
    m_default(defaultValue),
    m_source (std::move(source)),
    m_expectedValue(std::move(expectedValue))
{
}

CUserSettingColor* CUserSettingColor::fromJSON (nlohmann::json& data)
{
    bool hasCondition = false;
    bool hasSource = false;
    glm::vec3 defaultValue;
    std::string source;
    std::string expectedValue;

    if (data.is_object () == true)
    {
        hasSource = true;
        auto userIt = data.find ("user");
        defaultValue = WallpaperEngine::Core::aToColorf (jsonFindDefault <std::string> (data, "value", "").c_str ()); // is this default value right?

        if (userIt != data.end ())
        {
            if (userIt->is_string ())
            {
                source = *userIt;
            }
            else
            {
                hasCondition = true;
                source = *jsonFindRequired (userIt, "name", "Name for conditional setting must be present");
                expectedValue = *jsonFindRequired (userIt, "condition", "Condition for conditional setting must be present");
            }
        }
        else
        {
            fprintf (stderr, "Float color property doesn't have user member, this could mean an scripted value\n");
        }
    }
    else
    {
        if (data.is_string () == false)
            throw std::runtime_error ("Expected float color value on user settings");

        defaultValue = WallpaperEngine::Core::aToColorf (data.get <std::string> ().c_str ());
    }

    return new CUserSettingColor (hasCondition, hasSource, defaultValue, source, expectedValue);
}

CUserSettingColor* CUserSettingColor::fromScalar (glm::vec3 value)
{
    return new CUserSettingColor (false, false, value, "", "");
}

glm::vec3 CUserSettingColor::getDefaultValue ()
{
    return this->m_default;
}

glm::vec3 CUserSettingColor::processValue (const std::vector<Projects::CProperty*>& properties)
{
    if (this->m_hasSource == false && this->m_hasCondition == false)
        return this->getDefaultValue ();

    for (auto cur : properties)
    {
        if (cur->getName () != this->m_source)
            continue;

        if (this->m_hasCondition == false)
        {
            if (cur->is <CPropertyColor> ())
                return cur->as <CPropertyColor> ()->getValue ();

            throw std::runtime_error ("Property without condition must match type (color)");
        }

        throw std::runtime_error ("Color property with condition doesn't match against combo value");
    }

    return this->m_default;
}

std::string CUserSettingColor::Type = "color";