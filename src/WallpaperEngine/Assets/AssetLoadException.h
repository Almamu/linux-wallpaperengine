#pragma once

#include <filesystem>

namespace WallpaperEngine::Assets {
class AssetLoadException final : public std::filesystem::filesystem_error {
  public:
    using std::filesystem::filesystem_error::filesystem_error;
    explicit AssetLoadException (const std::filesystem::filesystem_error& filesystem_error) noexcept;
};
} // namespace WallpaperEngine::Assets