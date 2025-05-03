#pragma once

#include "CUserSettingValue.h"
#include "WallpaperEngine/Core/CProject.h"

namespace WallpaperEngine::Core::Projects {
class CProperty;
}

namespace WallpaperEngine::Core::UserSettings {
class CUserSettingBoolean : public CUserSettingValue {
  public:
    typedef bool data_type;

    static const CUserSettingBoolean* fromJSON (const nlohmann::json& data, const std::map <std::string, std::shared_ptr <Projects::CProperty>>& properties);
    static const CUserSettingBoolean* fromScalar (bool value);

  private:
    CUserSettingBoolean (
        bool hasCondition, bool defaultValue, std::shared_ptr <const Projects::CProperty> source, std::string expectedValue);

    const bool m_hasCondition;
    const std::string m_expectedValue;
    const std::shared_ptr <const Projects::CProperty> m_source;
};
} // namespace WallpaperEngine::Core::UserSettings