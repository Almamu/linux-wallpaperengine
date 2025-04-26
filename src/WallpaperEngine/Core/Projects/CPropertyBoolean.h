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

    [[nodiscard]] std::string dump () const override;
    void set (const std::string& value) override;
    [[nodiscard]] const char* getType () const override;

  private:
    CPropertyBoolean (bool value, std::string name, std::string text);
};
} // namespace WallpaperEngine::Core::Projects
