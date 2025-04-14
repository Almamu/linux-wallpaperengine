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
    static const CPropertyColor* fromJSON (const json& data, std::string name);

    /**
     * @return The RGB color value in the 0-1 range
     */
    [[nodiscard]] const glm::vec3& getValue () const;
    [[nodiscard]] std::string dump () const override;
    void update (const std::string& value) const override;

    static const std::string Type;

  private:
    CPropertyColor (glm::vec3 color, std::string name, std::string text);

    mutable glm::vec3 m_color;
};
} // namespace WallpaperEngine::Core::Projects
