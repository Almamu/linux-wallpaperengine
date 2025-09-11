#pragma once

#include <filesystem>
#include "WallpaperEngine/Data/Utils/BinaryReader.h"

namespace WallpaperEngine::FileSystem::Adapters {
using namespace WallpaperEngine::Data::Utils;
struct Adapter {
    Adapter () = default;
    virtual ~Adapter () = default;

    [[nodiscard]] virtual ReadStreamSharedPtr open (const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual bool exists (const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual std::filesystem::path realpath (const std::filesystem::path& path) const = 0;
};

using AdapterSharedPtr = std::shared_ptr<Adapter>;

struct Factory {
    Factory () = default;
    virtual ~Factory () = default;

    [[nodiscard]] virtual bool handlesMountpoint (const std::filesystem::path& path) const = 0;
    [[nodiscard]] virtual AdapterSharedPtr create (const std::filesystem::path& path) const = 0;
};

using FactoryUniquePtr = std::unique_ptr<Factory>;
} // namespace WallpaperEngine::FileSystem::Adapters