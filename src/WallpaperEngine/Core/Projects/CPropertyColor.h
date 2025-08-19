#pragma once

#include "CProperty.h"

#include "WallpaperEngine/Data/JSON.h"

namespace WallpaperEngine::Core::Projects {
using JSON = WallpaperEngine::Data::JSON::JSON;

/**
 * Represents a color property
 */
class CPropertyColor final : public CProperty {
  public:
    CPropertyColor (const std::string& color, std::string name, std::string text);

    static std::shared_ptr<CPropertyColor> fromJSON (const JSON& data, std::string name);
    [[nodiscard]] std::string dump () const override;
    void set (const std::string& value) override;

    [[nodiscard]] const char* getPropertyType () const override;
};
} // namespace WallpaperEngine::Core::Projects
