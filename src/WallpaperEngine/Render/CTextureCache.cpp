#include "CTextureCache.h"

using namespace WallpaperEngine::Render;

CTextureCache::CTextureCache (CRenderContext* context) :
    m_context (context)
{
}

CTextureCache::~CTextureCache ()
{
}

const ITexture* CTextureCache::resolve (const std::string& filename)
{
    auto found = this->m_textureCache.find (filename);

    if (found != this->m_textureCache.end ())
        return (*found).second;

    const ITexture* texture = this->m_context->getContainer ()->readTexture (filename);

    this->m_textureCache.insert (
        std::make_pair (filename, texture)
    );

    return texture;
}

void CTextureCache::store (std::string name, const ITexture* texture)
{
    this->m_textureCache.insert (
        std::make_pair (name, texture)
    );
}