#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;

/**
 * Represents a boolean property
 */
class CPropertyBoolean final : public CProperty {
  public:
    static CPropertyBoolean* fromJSON (const json& data, std::string name);

    /** @inheritdoc */
    [[nodiscard]] std::string dump () const override;
    /** @inheritdoc */
    void set (const std::string& value) override;

    static const std::string Type;

  private:
    CPropertyBoolean (bool value, std::string name, std::string text);
};
} // namespace WallpaperEngine::Core::Projects
