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
    static const CPropertyCombo* fromJSON (const json& data, std::string name);

    ~CPropertyCombo () override;

    /**
     * @return The selected value
     */
    [[nodiscard]] const std::string& getValue () const;
    [[nodiscard]] std::string dump () const override;
    void update (const std::string& value) const override;

    static const std::string Type;

  private:
    CPropertyCombo (
        std::string name, std::string text, std::string defaultValue, std::vector<const CPropertyComboValue*> values);

    /** List of values available to select */
    const std::vector<const CPropertyComboValue*> m_values;
    /** The default value */
    mutable std::string m_defaultValue;
};
} // namespace WallpaperEngine::Core::Projects
