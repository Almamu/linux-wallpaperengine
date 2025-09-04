#pragma once

#include <memory>
#include <vector>

namespace WallpaperEngine::Data::Assets {
struct Mipmap;
struct Frame;
struct Texture;

using MipmapSharedPtr = std::shared_ptr <Mipmap>;
using FrameSharedPtr = std::shared_ptr <Frame>;
using TextureUniquePtr = std::unique_ptr <Texture>;
using MipmapList = std::vector <MipmapSharedPtr>;
}