#pragma once

#include "WallpaperEngine/Data/Assets/Texture.h"

#include <functional>
#include <iostream>

#include "WallpaperEngine/Assets/AssetLocator.h"
#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Data/Model/Types.h"
#include "WallpaperEngine/Data/Utils/BinaryReader.h"

namespace WallpaperEngine::Data::Parsers {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Data::Utils;
using namespace WallpaperEngine::Data::Assets;
using namespace WallpaperEngine::Data::Model;

class TextureParser {
public:
    using VariantSelector = std::function<bool (const JSON&)>;
    static TextureUniquePtr parse (const BinaryReader& file);
    static TextureUniquePtr parse (const BinaryReader& file, const VariantSelector& selector, bool deferMipmaps = false);
    /** Compiled reference dimensions/flags without acquiring or decoding pixel payloads. */
    static TextureUniquePtr loadHeader (const WallpaperEngine::Assets::AssetLocator& locator, const std::string& filename);
    /** Prepared loads may retain static raw LZ4 until the required/optional decode batches. */
    static TextureUniquePtr load (const WallpaperEngine::Assets::AssetLocator& locator, const std::string& filename, bool deferMipmaps = false);
    static TextureUniquePtr parseImage (const std::string& contents, const JSON& metadata);
    static TextureUniquePtr selectVariants (const Texture& texture, const VariantSelector& selector, bool deferMipmaps = false);
    static std::vector<uint32_t> selectedVariants (const Texture& texture, const VariantSelector& selector);
    static bool matchesCondition (const JSON& condition, const Properties& properties);
    static TextureUniquePtr parse (
	const BinaryReader& file, const std::string& filename,
	std::function<std::string (const std::string&)> metadataLoader, bool deferMipmaps = false
    );
    static MipmapSharedPtr parseMipmap (const BinaryReader& file, const Texture& header, bool deferDecompression = false);
    static DecodedPixelsPtr decodeCompiledImage (FIF format, const char* data, int size, int& width, int& height);
    /**
     * Unpacks retained LZ4 and decodes image-format (FIF) mipmaps so the render
     * thread only has to upload them, meant to run off the render thread
     */
    static void decodeMipmaps (Texture& texture);
    /**
     * Same, for a whole batch of textures at once, decoding their mipmaps in parallel.
     * Mipmaps are independent, so a batch is the only place with enough of them to fill
     * the cores: a single texture holds 3.4 on average and its level 0 is most of the work
     */
    static void decodeMipmaps (const std::vector<Texture*>& textures, bool initialMipOnly = false);
    /** Bounded startup streaming: compiled static 2D images and explicitly prepared image variants. */
    static bool canStreamMipmaps (const Texture& texture);
    static FrameSharedPtr parseFrame (const BinaryReader& file);
    static FrameSharedPtr parseFrameV1 (const BinaryReader& file);
    static TextureMap parseTextureMap (const JSON& it);

private:
    static void parseTextureHeader (Texture& header, const BinaryReader& file);
    static void parseContainer (Texture& header, const BinaryReader& file);
    static void parseVariantPatches (
	Mipmap& mipmap, const Texture& header, const BinaryReader& file, const std::vector<uint32_t>& selected, bool deferPatches = false
    );
    static void parseAnimations (Texture& header, const BinaryReader& file);
    static void parseSpritesheetMetadata (
	Texture& header, const std::string& filename, std::function<std::string (const std::string&)> metadataLoader
    );
    static TextureFormat parseTextureFormat (uint32_t value);
    static uint32_t parseTextureFlags (uint32_t value);
    static FIF parseFIF (uint32_t value);
};
}
