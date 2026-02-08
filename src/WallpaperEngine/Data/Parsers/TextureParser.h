#pragma once

#include "WallpaperEngine/Data/Assets/Texture.h"

#include <functional>
#include <iostream>

#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Data/Utils/BinaryReader.h"

namespace WallpaperEngine::Data::Parsers {
using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Data::Assets;

class TextureParser {
public:
    static TextureUniquePtr parse (const BinaryReader& file);
    static TextureUniquePtr parse (
	const BinaryReader& file, const std::string& filename,
	std::function<std::string (const std::string&)> metadataLoader
    );
    static MipmapSharedPtr parseMipmap (const BinaryReader& file, const Texture& header);
    static FrameSharedPtr parseFrame (const BinaryReader& file);

private:
    static void parseTextureHeader (Texture& header, const BinaryReader& file);
    static void parseContainer (Texture& header, const BinaryReader& file);
    static void parseAnimations (Texture& header, const BinaryReader& file);
    static void parseSpritesheetMetadata (
	Texture& header, const std::string& filename, std::function<std::string (const std::string&)> metadataLoader
    );
    static TextureFormat parseTextureFormat (uint32_t value);
    static uint32_t parseTextureFlags (uint32_t value);
    static FIF parseFIF (uint32_t value);
};
}