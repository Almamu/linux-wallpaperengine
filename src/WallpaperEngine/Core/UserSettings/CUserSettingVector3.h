#pragma once

#include <glm/vec3.hpp>

#include "CUserSettingValue.h"

namespace WallpaperEngine::Core::Projects
{
    class CProperty;
}

namespace WallpaperEngine::Core::UserSettings
{
    class CUserSettingVector3 : public CUserSettingValue
    {
    public:
        static CUserSettingVector3* fromJSON (nlohmann::json& data);
        static CUserSettingVector3* fromScalar (glm::vec3 value);
        static std::string Type;

        glm::vec3 processValue (const std::vector<Projects::CProperty*>& properties);
        glm::vec3 getDefaultValue ();

    private:
        CUserSettingVector3 (bool hasCondition, bool hasSource, glm::vec3 defaultValue, std::string source, std::string expectedValue);

        glm::vec3 m_default;
        bool m_hasCondition;
        bool m_hasSource;
        std::string m_source;
        std::string m_expectedValue;
    };
}