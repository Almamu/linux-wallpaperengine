#include <glad/glad.h>

#include "CWallpaper.h"
#include "RenderContext.h"

#include "CTexture.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Context.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"

namespace WallpaperEngine::Render {
RenderContext::RenderContext (Context& context, Assets::AssetLocator& locator) :
    m_textureCache (*context.texture_cache), m_context (context), m_locator (locator) {
    // these textures are special cases, so make sure they're created only upon request
    this->m_currentThumbnail = std::make_shared<AlbumTexture> (this->getContext ());

#if !NDEBUG
    glObjectLabel (GL_TEXTURE, this->m_currentThumbnail->getTextureID (0), -1, "$mediaThumbnail");
#endif

    this->m_previousThumbnail = std::make_shared<AlbumTexture> (this->getContext ());

#if !NDEBUG
    glObjectLabel (GL_TEXTURE, this->m_previousThumbnail->getTextureID (0), -1, "$mediaPreviousThumbnail");
#endif

    // load the latest texture (if available)
    this->m_currentThumbnail->load ();

    // add these to the cache and return the right one
    this->store ("$mediaThumbnail", this->m_currentThumbnail);
    this->store ("$mediaPreviousThumbnail", this->m_previousThumbnail);

    this->m_mediaCallback = this->getContext ().getMediaSource ().addAlbumArtListener (
	[this] (const Media::MediaSource::MediaInfo& data) {
	    if (this->m_currentThumbnail->isReady ()) {
		// copy over pixel data and setup the new texture with the new data
		this->m_previousThumbnail->copyContents (*this->m_currentThumbnail);
	    }

	    // load the next image
	    this->m_currentThumbnail->load ();
	}
    );
}

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
