#pragma once

#include "CUserSettingValue.h"

namespace WallpaperEngine::Core::Projects {
class CProperty;
}

namespace WallpaperEngine::Core::UserSettings {
class CUserSettingFloat : public CUserSettingValue {
  public:
    typedef double data_type;

    static CUserSettingFloat* fromJSON (nlohmann::json& data);
    static CUserSettingFloat* fromScalar (double value);
    static std::string Type;

    double processValue (const std::vector<Projects::CProperty*>& properties);
    double getDefaultValue () const;

  private:
    CUserSettingFloat (bool hasCondition, bool hasSource, double defaultValue, std::string source,
                       std::string expectedValue);

    double m_default;
    bool m_hasCondition;
    bool m_hasSource;
    std::string m_source;
    std::string m_expectedValue;
};
} // namespace WallpaperEngine::Core::UserSettings