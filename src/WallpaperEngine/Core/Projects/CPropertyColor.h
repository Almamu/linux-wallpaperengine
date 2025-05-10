#pragma once

#include "CProperty.h"

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;

/**
 * Represents a color property
 */
class CPropertyColor final : public CProperty {
  public:
    CPropertyColor (const std::string& color, std::string name, std::string text);

    static std::shared_ptr<CPropertyColor> fromJSON (const json& data, std::string name);
    [[nodiscard]] std::string dump () const override;
    void set (const std::string& value) override;

    [[nodiscard]] const char* getType () const override;
};
} // namespace WallpaperEngine::Core::Projects
