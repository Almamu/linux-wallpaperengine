#include "CTextureCache.h"

#include "WallpaperEngine/Render/Helpers/CContextAware.h"
#include "WallpaperEngine/Assets/CAssetLoadException.h"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Assets;

CTextureCache::CTextureCache (CRenderContext& context) :
    Helpers::CContextAware (context)
{
}

CTextureCache::~CTextureCache ()
= default;

const ITexture* CTextureCache::resolve (const std::string& filename)
{
    auto found = this->m_textureCache.find (filename);

    if (found != this->m_textureCache.end ())
        return (*found).second;

    // search for the texture in all the different containers just in case
    for (auto it : this->getContext ().getApp ().getBackgrounds ())
    {
        try
        {
            const ITexture* texture = it.second->getContainer ()->readTexture (filename);

            this->store (filename, texture);

            return texture;
        }
        catch (CAssetLoadException& ex)
        {
            // ignored, this happens if we're looking at the wrong background
        }
    }

    if (this->getContext ().getApp ().getDefaultBackground () != nullptr)
    {
        try
        {
            const ITexture* texture = this->getContext ().getApp ().getDefaultBackground ()->getContainer ()->readTexture (filename);

            this->store (filename, texture);

            return texture;
        }
        catch (CAssetLoadException& ex)
        {
            // ignored, this happens if we're looking at the wrong background
        }
    }

    throw CAssetLoadException (filename, "Cannot find file");
}

void CTextureCache::store (const std::string& name, const ITexture* texture)
{
    this->m_textureCache.insert_or_assign (name, texture);
}