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
    static CPropertyColor* fromJSON (const json& data, std::string name);

    [[nodiscard]] std::string dump () const override;
    void set (const std::string& value) override;

    static const std::string Type;

  private:
    CPropertyColor (const std::string& color, std::string name, std::string text);
};
} // namespace WallpaperEngine::Core::Projects
