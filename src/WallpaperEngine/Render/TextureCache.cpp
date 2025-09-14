#include "TextureCache.h"

#include "WallpaperEngine/FileSystem/Container.h"

#include "CTexture.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Render/Helpers/ContextAware.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::FileSystem;
using namespace WallpaperEngine::Data::Parsers;

TextureCache::TextureCache (RenderContext& context) : Helpers::ContextAware (context) {}

std::shared_ptr<const TextureProvider> TextureCache::resolve (const std::string& filename) {
    if (const auto found = this->m_textureCache.find (filename); found != this->m_textureCache.end ())
        return found->second;

    auto finalFilename = std::filesystem::path("materials") / filename;
    finalFilename.replace_extension ("tex");

    // search for the texture in all the different containers just in case
    for (const auto& project : this->getContext ().getApp ().getBackgrounds () | std::views::values) {
        try {
            const auto contents = project->assetLocator->read (finalFilename);
            auto stream = BinaryReader (contents);

            auto parsedTexture = TextureParser::parse (stream);
            auto texture = std::make_shared <CTexture> (std::move (parsedTexture));

            this->store (filename, texture);

            return texture;
        } catch (AssetLoadException&) {
            // ignored, this happens if we're looking at the wrong background
        }
    }

    throw AssetLoadException ("Cannot find file", filename, std::error_code ());
}

void TextureCache::store (const std::string& name, std::shared_ptr<const TextureProvider> texture) {
    this->m_textureCache.insert_or_assign (name, texture);
}