#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;

/**
 * Represents different combo values
 */
class CPropertyComboValue {
  public:
    std::string label;
    std::string value;
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
    static CPropertyCombo* fromJSON (json data, const std::string& name);

    ~CPropertyCombo () override;

    /**
     * @return The selected value
     */
    [[nodiscard]] const std::string& getValue () const;
    [[nodiscard]] std::string dump () const override;
    void update (const std::string& value) override;

    static const std::string Type;

  private:
    CPropertyCombo (const std::string& name, const std::string& text, std::string defaultValue);

    /**
     * Adds a combo value to the list of possible values
     *
     * @param label
     * @param value
     */
    void addValue (std::string label, std::string value);

    /** List of values available to select */
    std::vector<CPropertyComboValue*> m_values;
    /** The default value */
    std::string m_defaultValue;
};
} // namespace WallpaperEngine::Core::Projects
