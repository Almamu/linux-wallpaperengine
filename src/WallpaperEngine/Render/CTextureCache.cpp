#include "CTextureCache.h"

using namespace WallpaperEngine::Render;

CTextureCache::CTextureCache (CRenderContext& context) :
    m_context (context)
{
}

CTextureCache::~CTextureCache ()
 = default;

const ITexture* CTextureCache::resolve (const std::string& filename)
{
    auto found = this->m_textureCache.find (filename);

    if (found != this->m_textureCache.end ())
        return (*found).second;

    const ITexture* texture = this->m_context.getContainer ().readTexture (filename);

    this->store (filename, texture);

    return texture;
}

void CTextureCache::store (const std::string& name, const ITexture* texture)
{
    this->m_textureCache.insert_or_assign (name, texture);
}