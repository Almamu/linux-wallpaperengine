#pragma once

#include <filesystem>
#include <utility>

#include "Types.h"

namespace WallpaperEngine::Media {
class MediaSource;
}
namespace WallpaperEngine::FileSystem::Adapters {
struct MediaCoverFactory final : Factory {
    MediaCoverFactory (Media::MediaSource& source) : source (source) { }

    [[nodiscard]] bool handlesMountpoint (const std::filesystem::path& path) const override;
    [[nodiscard]] AdapterSharedPtr create (const std::filesystem::path& path) const override;

    Media::MediaSource& source;
};

struct MediaCoverAdapter final : Adapter {
    explicit MediaCoverAdapter (Media::MediaSource& source) : source (source) { }

    [[nodiscard]] ReadStreamSharedPtr open (const std::filesystem::path& path) const override;
    [[nodiscard]] bool exists (const std::filesystem::path& path) const override;
    [[nodiscard]] std::filesystem::path physicalPath (const std::filesystem::path& path) const override;

    Media::MediaSource& source;
};
}
