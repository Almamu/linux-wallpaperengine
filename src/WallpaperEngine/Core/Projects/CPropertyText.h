#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects {
using json = nlohmann::json;

/**
 * Represents a text property
 */
class CPropertyText final : public CProperty {
  public:
    CPropertyText (std::string name, std::string text);

    static std::shared_ptr<CPropertyText> fromJSON (const json& data, std::string name);
    [[nodiscard]] std::string dump () const override;
    void set (const std::string& value) override;

    [[nodiscard]] const char* getType () const override;
  private:
};
} // namespace WallpaperEngine::Core::Projects
