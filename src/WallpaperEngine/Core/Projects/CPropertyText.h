#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects {
using JSON = WallpaperEngine::Data::JSON::JSON;

/**
 * Represents a text property
 */
class CPropertyText final : public CProperty {
  public:
    CPropertyText (std::string name, std::string text);

    static std::shared_ptr<CPropertyText> fromJSON (const JSON& data, std::string name);
    [[nodiscard]] std::string dump () const override;
    void set (const std::string& value) override;

    [[nodiscard]] const char* getPropertyType () const override;
  private:
};
} // namespace WallpaperEngine::Core::Projects
