#pragma once

#include <glm/vec3.hpp>

#include "CUserSettingValue.h"

namespace WallpaperEngine::Core::Projects
{
    class CProperty;
}

namespace WallpaperEngine::Core::UserSettings
{
    class CUserSettingColor : public CUserSettingValue
    {
    public:
        static CUserSettingColor* fromJSON (nlohmann::json& data);
        static CUserSettingColor* fromScalar (glm::vec3 value);
        static std::string Type;

        glm::vec3 processValue (const std::vector<Projects::CProperty*>& properties);
        glm::vec3 getDefaultValue ();

    private:
        CUserSettingColor (bool hasCondition, bool hasSource, glm::vec3 defaultValue, std::string source, std::string expectedValue);

        glm::vec3 m_default;
        bool m_hasCondition;
        bool m_hasSource;
        std::string m_source;
        std::string m_expectedValue;
    };
}