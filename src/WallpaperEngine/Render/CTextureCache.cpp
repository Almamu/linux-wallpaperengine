#include "CTextureCache.h"

#include "WallpaperEngine/Assets/CAssetLoadException.h"
#include "WallpaperEngine/Render/Helpers/CContextAware.h"

#include "WallpaperEngine/Data/Model/Project.h"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Assets;

CTextureCache::CTextureCache (CRenderContext& context) : Helpers::CContextAware (context) {}

std::shared_ptr<const ITexture> CTextureCache::resolve (const std::string& filename) {
    const auto found = this->m_textureCache.find (filename);

    if (found != this->m_textureCache.end ())
        return found->second;

    // search for the texture in all the different containers just in case
    for (const auto& [name, project] : this->getContext ().getApp ().getBackgrounds ()) {
        try {
            std::shared_ptr<const ITexture> texture = project->container->readTexture (filename);

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