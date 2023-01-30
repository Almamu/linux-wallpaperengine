#pragma once

#include "CUserSettingValue.h"

namespace WallpaperEngine::Core::Projects
{
    class CProperty;
}

namespace WallpaperEngine::Core::UserSettings
{
    class CUserSettingBoolean : public CUserSettingValue
    {
    public:
        static CUserSettingBoolean* fromJSON (nlohmann::json& data);
        static CUserSettingBoolean* fromScalar (bool value);
        static std::string Type;

        bool processValue (const std::vector<Projects::CProperty*>& properties);
        bool getDefaultValue ();

    private:
        CUserSettingBoolean (bool hasCondition, bool hasSource, bool defaultValue, std::string source, std::string expectedValue);

        bool m_default;
        bool m_hasCondition;
        bool m_hasSource;
        std::string m_source;
        std::string m_expectedValue;
    };
}