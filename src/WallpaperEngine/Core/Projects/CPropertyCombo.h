#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;

/**
 * Represents different combo values
 */
struct CPropertyComboValue {
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
    static std::shared_ptr<CPropertyCombo> fromJSON (const json& data, std::string name);

    CPropertyCombo (
        std::string name, std::string text, const std::string& defaultValue,
        std::vector<CPropertyComboValue> values);

    [[nodiscard]] std::string dump () const override;
    void set (const std::string& value) override;
    int translateValueToIndex (const std::string& value) const;

    [[nodiscard]] const char* getType () const override;

  private:
    /** List of values available to select */
    const std::vector<CPropertyComboValue> m_values;
};
} // namespace WallpaperEngine::Core::Projects
