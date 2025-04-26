#pragma once

#include "CUserSettingValue.h"
#include "WallpaperEngine/Core/CProject.h"

namespace WallpaperEngine::Core::Projects {
class CProperty;
}

namespace WallpaperEngine::Core::UserSettings {
class CUserSettingFloat : public CUserSettingValue {
  public:
    typedef float data_type;

    static const CUserSettingFloat* fromJSON (const nlohmann::json& data, const CProject& project);
    static const CUserSettingFloat* fromScalar (float value);

  private:
    CUserSettingFloat (
        bool hasCondition, float defaultValue, const Projects::CProperty* source, std::string expectedValue);

    const double m_default;
    const bool m_hasCondition;
    const Projects::CProperty* m_source;
    const std::string m_expectedValue;
};
} // namespace WallpaperEngine::Core::UserSettings