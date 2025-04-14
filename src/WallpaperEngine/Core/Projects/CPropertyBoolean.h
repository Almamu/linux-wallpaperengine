#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;

/**
 * Represents a boolean property
 */
class CPropertyBoolean final : public CProperty {
  public:
    static const CPropertyBoolean* fromJSON (const json& data, std::string name);

    /**
     * @return The value of the property
     */
    [[nodiscard]] bool getValue () const;
    /** @inheritdoc */
    [[nodiscard]] std::string dump () const override;
    /** @inheritdoc */
    void update (const std::string& value) const override;

    static const std::string Type;

  private:
    CPropertyBoolean (bool value, std::string name, std::string text);

    /** Property's value */
    mutable bool m_value;
};
} // namespace WallpaperEngine::Core::Projects
