#include "CTextureCache.h"

#include "WallpaperEngine/Assets/CAssetLoadException.h"

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Assets;

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

	// search for the texture in all the different containers just in case
	for (auto it : this->m_context.getApp ().getProjects ())
	{
		const ITexture* texture = it.second->getContainer ()->readTexture (filename);

		this->store (filename, texture);

		return texture;
	}

	if (this->m_context.getApp ().getDefaultProject () != nullptr)
	{
		const ITexture* texture = this->m_context.getApp ().getDefaultProject ()->getContainer ()->readTexture (filename);

		this->store (filename, texture);

		return texture;
	}

	throw CAssetLoadException (filename, "Cannot find file");
}

void CTextureCache::store (std::string name, const ITexture* texture)
{
    this->m_textureCache.insert_or_assign (name, texture);
}