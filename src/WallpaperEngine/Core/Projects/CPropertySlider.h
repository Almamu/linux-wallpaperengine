#pragma once

#include "CProperty.h"

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;

/**
 * Represents a slider value with a minimum and maximum value
 */
class CPropertySlider final : public CProperty {
  public:
    static const CPropertySlider* fromJSON (const json& data, const std::string& name);

    /**
     * @return The slider's value
     */
    [[nodiscard]] const double& getValue () const;
    /**
     * @return The slider's minimum value
     */
    [[nodiscard]] const double& getMinValue () const;
    /**
     * @return The slider's maximum value
     */
    [[nodiscard]] const double& getMaxValue () const;
    /**
     * @return The slider's value increment steps, only really used in the UI
     */
    [[nodiscard]] const double& getStep () const;
    [[nodiscard]] std::string dump () const override;
    void update (const std::string& value) const override;

    static const std::string Type;

  private:
    CPropertySlider (double value, const std::string& name, const std::string& text, double min, double max, double step);

    /** Actual slider value */
    mutable double m_value;
    /** Minimum value */
    const double m_min;
    /** Maximum value */
    const double m_max;
    /** Increment steps for the slider in the UI */
    const double m_step;
};
} // namespace WallpaperEngine::Core::Projects
