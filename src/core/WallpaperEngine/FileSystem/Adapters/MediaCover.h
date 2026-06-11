#pragma once

#include <filesystem>
#include <utility>

#include "Types.h"
#include "WallpaperEngine/Project.h"

namespace WallpaperEngine {
class Context;
}
namespace WallpaperEngine::Media {
class MediaSource;
}
namespace WallpaperEngine::FileSystem::Adapters {
struct MediaCoverFactory final : Factory {
    MediaCoverFactory (const Project& project) : project (project) { }

    [[nodiscard]] bool handlesMountpoint (const std::filesystem::path& path) const override;
    [[nodiscard]] AdapterSharedPtr create (const std::filesystem::path& path) const override;

    const Project& project;
};

struct MediaCoverAdapter final : Adapter {
    explicit MediaCoverAdapter (const Project& project) : project (project) { }

    [[nodiscard]] ReadStreamSharedPtr open (const std::filesystem::path& path) const override;
    [[nodiscard]] bool exists (const std::filesystem::path& path) const override;
    [[nodiscard]] std::filesystem::path physicalPath (const std::filesystem::path& path) const override;

    const Project& project;
};
}
