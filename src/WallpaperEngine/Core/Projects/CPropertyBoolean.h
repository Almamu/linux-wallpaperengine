#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;

/**
 * Represents a boolean property
 */
class CPropertyBoolean final : public CProperty {
  public:
    CPropertyBoolean (bool value, std::string name, std::string text);

    static std::shared_ptr<CPropertyBoolean> fromJSON (const json& data, std::string name);
    [[nodiscard]] std::string dump () const override;
    void set (const std::string& value) override;

    [[nodiscard]] const char* getType () const override;
};
} // namespace WallpaperEngine::Core::Projects
