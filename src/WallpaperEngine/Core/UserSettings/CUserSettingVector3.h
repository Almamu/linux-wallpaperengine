#pragma once

#include <glm/vec3.hpp>

#include "CUserSettingValue.h"

namespace WallpaperEngine::Core::Projects {
class CProperty;
}

namespace WallpaperEngine::Core::UserSettings {
class CUserSettingVector3 : public CUserSettingValue {
  public:
    typedef glm::vec3 data_type;

    static const CUserSettingVector3* fromJSON (const nlohmann::json& data);
    static const CUserSettingVector3* fromScalar (const glm::vec3 value);
    static std::string Type;

    [[nodiscard]] const glm::vec3& processValue (const std::vector<const Projects::CProperty*>& properties) const;
    [[nodiscard]] const glm::vec3& getDefaultValue () const;

  private:
    CUserSettingVector3 (
        bool hasCondition, bool hasSource, glm::vec3 defaultValue, std::string source, std::string expectedValue);

    const glm::vec3 m_default;
    const bool m_hasCondition;
    const bool m_hasSource;
    const std::string m_source;
    const std::string m_expectedValue;
};
} // namespace WallpaperEngine::Core::UserSettings