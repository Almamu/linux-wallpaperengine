#pragma once

#include <memory>
#include <vector>

namespace WallpaperEngine::Data::Assets {
struct Mipmap;
struct Frame;
struct Texture;
struct Package;
struct FileEntry;

using MipmapSharedPtr = std::shared_ptr <Mipmap>;
using FrameSharedPtr = std::shared_ptr <Frame>;
using TextureUniquePtr = std::unique_ptr <Texture>;
using FileEntryUniquePtr = std::unique_ptr <FileEntry>;
using PackageUniquePtr = std::unique_ptr <Package>;
using MipmapList = std::vector <MipmapSharedPtr>;
using FileEntryList = std::vector <FileEntryUniquePtr>;
}