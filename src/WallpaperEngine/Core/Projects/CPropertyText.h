#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;

/**
 * Represents a text property
 */
class CPropertyText final : public CProperty {
  public:
    static CPropertyText* fromJSON (const json& data, std::string name);
    [[nodiscard]] std::string dump () const override;
    void set (const std::string& value) override;

    static const std::string Type;

  private:
    CPropertyText (std::string name, std::string text);
};
} // namespace WallpaperEngine::Core::Projects
