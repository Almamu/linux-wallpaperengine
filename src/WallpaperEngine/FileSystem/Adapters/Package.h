#pragma once

#include <filesystem>
#include <utility>

#include "Types.h"
#include "WallpaperEngine/Data/Assets/Types.h"
#include "WallpaperEngine/Data/Assets/Package.h"

namespace WallpaperEngine::FileSystem::Adapters {
using namespace WallpaperEngine::Data::Assets;

struct PackageFactory final : Factory {
    PackageFactory () = default;

    [[nodiscard]] bool handlesMountpoint (const std::filesystem::path& path) const override;
    [[nodiscard]] AdapterSharedPtr create (const std::filesystem::path& path) const override;
};

struct PackageAdapter final : Adapter {
    explicit PackageAdapter (PackageUniquePtr package) : package (std::move(package)) {};

    [[nodiscard]] ReadStreamSharedPtr open (const std::filesystem::path& path) const override;
    [[nodiscard]] bool exists (const std::filesystem::path& path) const override;
    [[nodiscard]] std::filesystem::path realpath (const std::filesystem::path& path) const override;

    PackageUniquePtr package;
};
}