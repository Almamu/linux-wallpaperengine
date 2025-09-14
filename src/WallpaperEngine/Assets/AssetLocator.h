#pragma once

#include "WallpaperEngine/FileSystem/Container.h"

namespace WallpaperEngine::Assets {
using namespace WallpaperEngine::FileSystem;
using namespace WallpaperEngine::Data::Model;
class AssetLocator {
  public:
    explicit AssetLocator (ContainerUniquePtr filesystem);

    std::string vertexShader (const std::filesystem::path& filename) const;
    std::string fragmentShader (const std::filesystem::path& filename) const;
    std::string includeShader (const std::filesystem::path& filename) const;
    ReadStreamSharedPtr texture (const std::filesystem::path& filename) const;
    std::string readString (const std::filesystem::path& filename) const;
    ReadStreamSharedPtr read (const std::filesystem::path& path) const;
    std::filesystem::path physicalPath (const std::filesystem::path& path) const;

  private:
    std::string shader (const std::filesystem::path& filename) const;

    ContainerUniquePtr m_filesystem;
};

using AssetLocatorUniquePtr = std::unique_ptr<AssetLocator>;
}