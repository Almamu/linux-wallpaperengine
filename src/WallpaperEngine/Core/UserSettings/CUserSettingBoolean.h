#pragma once

#include "CUserSettingValue.h"

namespace WallpaperEngine::Core::Projects {
class CProperty;
}

namespace WallpaperEngine::Core::UserSettings {
class CUserSettingBoolean : public CUserSettingValue {
  public:
    typedef bool data_type;

    static const CUserSettingBoolean* fromJSON (const nlohmann::json& data);
    static const CUserSettingBoolean* fromScalar (const bool value);
    static std::string Type;

    [[nodiscard]] bool processValue (const std::vector<const Projects::CProperty*>& properties) const;
    [[nodiscard]] bool getDefaultValue () const;

  private:
    CUserSettingBoolean (
        bool hasCondition, bool hasSource, bool defaultValue, std::string source, std::string expectedValue);

    const bool m_default;
    const bool m_hasCondition;
    const bool m_hasSource;
    const std::string m_source;
    const std::string m_expectedValue;
};
} // namespace WallpaperEngine::Core::UserSettings