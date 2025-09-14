#pragma once

#include <filesystem>
#include <utility>

#include "Types.h"

#include "WallpaperEngine/Data/Utils/MemoryStream.h"
#include "WallpaperEngine/Data/JSON.h"
#include <map>

namespace WallpaperEngine::FileSystem::Adapters {
using JSON = WallpaperEngine::Data::JSON::JSON;

struct VirtualFactory final : Factory {
    [[nodiscard]] bool handlesMountpoint (const std::filesystem::path& path) const override;
    [[nodiscard]] AdapterSharedPtr create (const std::filesystem::path& path) const override;
};

struct VirtualAdapter final : Adapter {
    [[nodiscard]] ReadStreamSharedPtr open (const std::filesystem::path& path) const override;
    [[nodiscard]] bool exists (const std::filesystem::path& path) const override;
    [[nodiscard]] std::filesystem::path physicalPath (const std::filesystem::path& path) const override;

    void add (const std::filesystem::path& path, const char* data);
    void add (const std::filesystem::path& path, const JSON& data);
    void add (const std::filesystem::path& path, const std::string& data);
    void add (const std::filesystem::path& path, MemoryStreamSharedPtr stream);

    std::map <std::filesystem::path, MemoryStreamSharedPtr> files;
};
}