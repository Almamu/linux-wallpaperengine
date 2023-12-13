#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;

/**
 * Represents a boolean property
 */
class CPropertyBoolean final : public CProperty {
  public:
    static CPropertyBoolean* fromJSON (json data, const std::string& name);

    /**
     * @return The value of the property
     */
    [[nodiscard]] bool getValue () const;
    /** @inheritdoc */
    [[nodiscard]] std::string dump () const override;
    /** @inheritdoc */
    void update (const std::string& value) override;

    static const std::string Type;

  private:
    CPropertyBoolean (bool value, const std::string& name, const std::string& text);

    /** Property's value */
    bool m_value;
};
} // namespace WallpaperEngine::Core::Projects
