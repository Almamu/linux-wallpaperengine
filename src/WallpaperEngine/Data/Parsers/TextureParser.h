#pragma once

#include "WallpaperEngine/Data/Assets/Texture.h"

#include <iostream>

#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Data/Utils/BinaryReader.h"

namespace WallpaperEngine::Data::Parsers {
using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Data::Assets;

class TextureParser {
  public:
    static TextureUniquePtr parse (BinaryReader& file);
    static MipmapSharedPtr parseMipmap (BinaryReader& file, Texture& header);
    static FrameSharedPtr parseFrame (BinaryReader& file, Texture& header);

  private:
    static TextureFormat parseTextureFormat (uint32_t value);
    static uint32_t parseTextureFlags (uint32_t value);
    static FIF parseFIF (uint32_t value);
};
}