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
    CPropertySlider (float value, const std::string& name, const std::string& text, float min, float max, float step);

    static std::shared_ptr<CPropertySlider> fromJSON (const json& data, const std::string& name);
    /**
     * @return The slider's minimum value
     */
    [[nodiscard]] const float& getMinValue () const;
    /**
     * @return The slider's maximum value
     */
    [[nodiscard]] const float& getMaxValue () const;
    /**
     * @return The slider's value increment steps, only really used in the UI
     */
    [[nodiscard]] const float& getStep () const;
    [[nodiscard]] std::string dump () const override;
    void set (const std::string& value) override;

    [[nodiscard]] const char* getType () const override;

  private:
    /** Minimum value */
    const float m_min;
    /** Maximum value */
    const float m_max;
    /** Increment steps for the slider in the UI */
    const float m_step;
};
} // namespace WallpaperEngine::Core::Projects
