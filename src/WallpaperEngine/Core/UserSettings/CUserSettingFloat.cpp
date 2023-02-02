#include "common.h"
#include "CUserSettingFloat.h"
#include "WallpaperEngine/Core/Core.h"

#include "WallpaperEngine/Core/Projects/CProperty.h"
#include "WallpaperEngine/Core/Projects/CPropertySlider.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Projects;
using namespace WallpaperEngine::Core::UserSettings;

CUserSettingFloat::CUserSettingFloat (bool hasCondition, bool hasSource, double defaultValue, std::string source, std::string expectedValue) :
    CUserSettingValue (Type),
    m_hasCondition (hasCondition),
    m_hasSource(hasSource),
    m_default(defaultValue),
    m_source (std::move(source)),
    m_expectedValue(std::move(expectedValue))
{
}

CUserSettingFloat* CUserSettingFloat::fromJSON (nlohmann::json& data)
{
    bool hasCondition = false;
    bool hasSource = false;
    double defaultValue = false;
    std::string source;
    std::string expectedValue;

    if (data.is_object () == true)
    {
        hasSource = true;
        auto userIt = data.find ("user");
        defaultValue = jsonFindDefault (data, "value", 1.0); // is this default value right?

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
            sLog.error ("Float property doesn't have user member, this could mean an scripted value");
        }
    }
    else
    {
        if (data.is_number () == false)
            sLog.exception ("Expected numeric value on user settings");

        defaultValue = data.get<double> ();
    }

    return new CUserSettingFloat (hasCondition, hasSource, defaultValue, source, expectedValue);
}

CUserSettingFloat* CUserSettingFloat::fromScalar (double value)
{
    return new CUserSettingFloat (false, false, value, "", "");
}

double CUserSettingFloat::getDefaultValue ()
{
    return this->m_default;
}

double CUserSettingFloat::processValue (const std::vector<Projects::CProperty*>& properties)
{
    if (this->m_hasSource == false && this->m_hasCondition == false)
        return this->getDefaultValue ();

    for (auto cur : properties)
    {
        if (cur->getName () != this->m_source)
            continue;

        if (this->m_hasCondition == false)
        {
            if (cur->is <CPropertySlider> ())
                return cur->as <CPropertySlider> ()->getValue ();

            sLog.exception ("Property without condition must match type (slider)");
        }

        sLog.exception ("Float property with condition doesn't match against combo value");
    }

    return this->m_default;
}

std::string CUserSettingFloat::Type = "float";