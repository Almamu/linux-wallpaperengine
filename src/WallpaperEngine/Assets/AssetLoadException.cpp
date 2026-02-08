#include "AssetLoadException.h"

using namespace WallpaperEngine::Assets;

AssetLoadException::AssetLoadException (const std::filesystem::filesystem_error& filesystem_error) noexcept :
    std::filesystem::filesystem_error (filesystem_error) { }
