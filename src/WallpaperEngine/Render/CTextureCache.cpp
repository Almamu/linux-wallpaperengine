#include "CTextureCache.h"

#include "WallpaperEngine/FileSystem/Container.h"

#include "CTexture.h"
#include "WallpaperEngine/Assets/CAssetLoadException.h"
#include "WallpaperEngine/Render/Helpers/CContextAware.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::FileSystem;
using namespace WallpaperEngine::Data::Parsers;

CTextureCache::CTextureCache (CRenderContext& context) : Helpers::CContextAware (context) {}

std::shared_ptr<const ITexture> CTextureCache::resolve (const std::string& filename) {
    const auto found = this->m_textureCache.find (filename);

    if (found != this->m_textureCache.end ())
        return found->second;

    auto finalFilename = std::filesystem::path("materials") / filename;
    finalFilename.replace_extension ("tex");

    // search for the texture in all the different containers just in case
    for (const auto& [name, project] : this->getContext ().getApp ().getBackgrounds ()) {
        try {
            auto contents = project->container->read (finalFilename);
            auto stream = BinaryReader (contents);

            auto parsedTexture = TextureParser::parse (stream);
            auto texture = std::make_shared <CTexture> (std::move (parsedTexture));

            this->store (filename, texture);

            return texture;
        } catch (CAssetLoadException&) {
            // ignored, this happens if we're looking at the wrong background
        }
    }

    throw CAssetLoadException (filename, "Cannot find file");
}

void CTextureCache::store (const std::string& name, std::shared_ptr<const ITexture> texture) {
    this->m_textureCache.insert_or_assign (name, texture);
}