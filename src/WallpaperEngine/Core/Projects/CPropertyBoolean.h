#pragma once

#include "CProperty.h"

namespace WallpaperEngine::Core::Projects {
using JSON = WallpaperEngine::Data::JSON::JSON;

/**
 * Represents a boolean property
 */
class CPropertyBoolean final : public CProperty {
  public:
    CPropertyBoolean (bool value, std::string name, std::string text);

    static std::shared_ptr<CPropertyBoolean> fromJSON (const JSON& data, std::string name);
    [[nodiscard]] std::string dump () const override;
    void set (const std::string& value) override;

    [[nodiscard]] const char* getPropertyType () const override;
};
} // namespace WallpaperEngine::Core::Projects
