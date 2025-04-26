#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;

/**
 * Represents different combo values
 */
class CPropertyComboValue {
  public:
    const std::string label;
    const std::string value;
};

/**
 * Represents a combo property
 *
 * Combos are properties that have different values available and only one can be selected at once
 * this limits the user's possibilities, used for things like the amount of samples to use in audioprocessing
 * backgrounds
 */
class CPropertyCombo final : public CProperty {
  public:
    static CPropertyCombo* fromJSON (const json& data, std::string name);

    ~CPropertyCombo () override;

    [[nodiscard]] std::string dump () const override;
    void set (const std::string& value) override;
    int translateValueToIndex (const std::string& value) const;
    [[nodiscard]] const char* getType () const override;

  private:
    CPropertyCombo (
        std::string name, std::string text, const std::string& defaultValue, std::vector<const CPropertyComboValue*> values);

    /** List of values available to select */
    const std::vector<const CPropertyComboValue*> m_values;
};
} // namespace WallpaperEngine::Core::Projects
