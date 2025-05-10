#pragma once

#include <glm/vec3.hpp>

#include "CUserSettingValue.h"
#include "WallpaperEngine/Core/CProject.h"

namespace WallpaperEngine::Core::Projects {
class CProperty;
}

namespace WallpaperEngine::Core::UserSettings {
class CUserSettingVector3 : public CUserSettingValue {
  public:
    typedef glm::vec3 data_type;

    static const CUserSettingVector3* fromJSON (const nlohmann::json& data, const CProject& project);
    static const CUserSettingVector3* fromScalar (glm::vec3 value);

  private:
    CUserSettingVector3 (
        bool hasCondition, glm::vec3 defaultValue, std::shared_ptr<const Projects::CProperty> source,
        std::string expectedValue);

    const bool m_hasCondition;
    const std::shared_ptr <const Projects::CProperty> m_source;
    const std::string m_expectedValue;
};
} // namespace WallpaperEngine::Core::UserSettings