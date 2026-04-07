#include <glad/glad.h>

#include "CWallpaper.h"
#include "RenderContext.h"

#include "CTexture.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Context.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"

namespace WallpaperEngine::Render {
RenderContext::RenderContext (Context& context, Assets::AssetLocator& locator) :
	m_textureCache (*context.texture_cache), m_context (context), m_locator (locator) { }

const Context& RenderContext::getContext () const { return this->m_context; }

std::shared_ptr<const TextureProvider> RenderContext::resolveTexture (const std::string& name) {
	try {
		return this->m_textureCache.resolve (name);
	} catch (const Assets::AssetLoadException& e) {
		const auto& locator = this->getAssetLocator ();
		const auto contents = locator.texture (name);
		const auto stream = BinaryReader (contents);

		auto metadataLoader = [&locator] (const std::string& filename) -> std::string {
			return locator.readString (std::filesystem::path ("materials") / filename);
		};

		auto parsedTexture = Data::Parsers::TextureParser::parse (stream, name, metadataLoader);
		auto texture = std::make_shared<CTexture> (*this, std::move (parsedTexture));

		this->m_textureCache.store (name, texture);

		return texture;
	}
}

const Assets::AssetLocator& RenderContext::getAssetLocator () const { return this->m_locator; }

} // namespace WallpaperEngine::Render
