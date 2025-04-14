#pragma once

#include "CUserSettingValue.h"

namespace WallpaperEngine::Core::Projects {
class CProperty;
}

namespace WallpaperEngine::Core::UserSettings {
class CUserSettingFloat : public CUserSettingValue {
  public:
    typedef double data_type;

    static const CUserSettingFloat* fromJSON (const nlohmann::json& data);
    static const CUserSettingFloat* fromScalar (const double value);
    static std::string Type;

    [[nodiscard]] double processValue (const std::vector<const Projects::CProperty*>& properties) const;
    [[nodiscard]] double getDefaultValue () const;

  private:
    CUserSettingFloat (
        bool hasCondition, bool hasSource, double defaultValue, std::string source, std::string expectedValue);

    const double m_default;
    const bool m_hasCondition;
    const bool m_hasSource;
    const std::string m_source;
    const std::string m_expectedValue;
};
} // namespace WallpaperEngine::Core::UserSettings