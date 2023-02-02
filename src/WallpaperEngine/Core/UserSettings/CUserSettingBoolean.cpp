#include "common.h"
#include "CUserSettingBoolean.h"
#include "WallpaperEngine/Core/Core.h"

#include "WallpaperEngine/Core/Projects/CProperty.h"
#include "WallpaperEngine/Core/Projects/CPropertyBoolean.h"
#include "WallpaperEngine/Core/Projects/CPropertyCombo.h"
#include "WallpaperEngine/Core/Projects/CPropertySlider.h"
#include "WallpaperEngine/Core/Projects/CPropertyText.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Projects;
using namespace WallpaperEngine::Core::UserSettings;

CUserSettingBoolean::CUserSettingBoolean (bool hasCondition, bool hasSource, bool defaultValue, std::string source, std::string expectedValue) :
    CUserSettingValue (Type),
    m_hasCondition (hasCondition),
    m_hasSource(hasSource),
    m_default(defaultValue),
    m_source (std::move(source)),
    m_expectedValue(std::move(expectedValue))
{
}

CUserSettingBoolean* CUserSettingBoolean::fromJSON (nlohmann::json& data)
{
    bool hasCondition = false;
    bool hasSource = false;
    bool defaultValue = false;
    std::string source;
    std::string expectedValue;

    if (data.is_object () == true)
    {
        hasSource = true;
        auto userIt = data.find ("user");
        defaultValue = jsonFindDefault (data, "value", false); // is this default value right?

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
            sLog.error ("Boolean property doesn't have user member, this could mean an scripted value");
        }
    }
    else
    {
        if (data.is_boolean () == false)
            sLog.error ("Expected boolean value on user setting");

        defaultValue = data.get<bool> ();
    }

    return new CUserSettingBoolean (hasCondition, hasSource, defaultValue, source, expectedValue);
}

CUserSettingBoolean* CUserSettingBoolean::fromScalar (bool value)
{
    return new CUserSettingBoolean (false, false, value, "", "");
}

bool CUserSettingBoolean::getDefaultValue ()
{
    return this->m_default;
}

bool CUserSettingBoolean::processValue (const std::vector<Projects::CProperty*>& properties)
{
    if (this->m_hasSource == false && this->m_hasCondition == false)
        return this->getDefaultValue ();

    for (auto cur : properties)
    {
        if (cur->getName () != this->m_source)
            continue;

        if (this->m_hasCondition == false)
        {
            if (cur->is <CPropertyBoolean> ())
                return cur->as <CPropertyBoolean> ()->getValue ();

            sLog.exception ("Property without condition must match type boolean");
        }

        // TODO: properly validate this as the combos might be more than just strings?
        if (cur->is <CPropertyCombo> ())
            return cur->as <CPropertyCombo> ()->getValue () == this->m_expectedValue;

        sLog.exception ("Boolean property with condition doesn't match against combo value");
    }

    return this->m_default;
}

std::string CUserSettingBoolean::Type = "boolean";