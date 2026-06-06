#include "TextureCache.h"

#include "AlbumTexture.h"
#include "WallpaperEngine/FileSystem/Container.h"

#include "CTexture.h"
#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Render/Helpers/ContextAware.h"

#include "WallpaperEngine/Data/Model/Project.h"
#include "WallpaperEngine/Data/Parsers/TextureParser.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::FileSystem;
using namespace WallpaperEngine::Data::Parsers;
using namespace WallpaperEngine::Data::Assets;

TextureCache::TextureCache (RenderContext& context) : Helpers::ContextAware (context) {
    this->m_mediaCallback
	= this->getContext ().getMediaSource ().addListener ([this] (Media::MediaSource::MediaInfo& data) {
	      if (this->m_currentThumbnail == nullptr || this->m_previousThumbnail == nullptr) {
		  return;
	      }

	      // copy over pixel data and setup the new texture with the new data
	      this->m_previousThumbnail->swap (*this->m_currentThumbnail);
	      // finally load the new image
	      this->m_currentThumbnail->load (data);
	  });
}

TextureCache::~TextureCache () { this->m_mediaCallback (); }

std::shared_ptr<const TextureProvider> TextureCache::resolve (const std::string& filename) {
    if (const auto found = this->m_textureCache.find (filename); found != this->m_textureCache.end ()) {
	return found->second;
    }

    if (filename == "$mediaThumbnail" || filename == "$mediaPreviousThumbnail") {
	// these textures are special cases, so make sure they're created only upon request
	this->m_currentThumbnail = std::make_shared<AlbumTexture> (this->getContext ());
	this->m_previousThumbnail = std::make_shared<AlbumTexture> (this->getContext ());

	// add these to the cache and return the right one
	this->store ("$mediaThumbnail", this->m_currentThumbnail);
	this->store ("$mediaPreviousThumbnail", this->m_previousThumbnail);

	return filename == "$mediaThumbnail" ? this->m_currentThumbnail : this->m_previousThumbnail;
    }

    // search for the texture in all the different containers just in case
    for (const auto& project : this->getContext ().getApp ().getBackgrounds () | std::views::values) {
	try {
	    const auto contents = project->assetLocator->texture (filename);
	    auto stream = BinaryReader (contents);

	    // Create metadata loader lambda that captures the assetLocator
	    // so we need to construct the full path here
	    auto metadataLoader = [&project] (const std::string& metaFilename) -> std::string {
		std::filesystem::path fullPath = std::filesystem::path ("materials") / metaFilename;
		return project->assetLocator->readString (fullPath);
	    };

	    auto parsedTexture = TextureParser::parse (stream, filename, metadataLoader);
	    auto texture = std::make_shared<CTexture> (this->getContext (), std::move (parsedTexture));

	    this->store (filename, texture);

	    return texture;
	} catch (AssetLoadException&) {
	    // ignored, this happens if we're looking at the wrong background
	}
    }

    // TODO: FILL IN WITH A CHECKERED PATTERN TEXTURE INSTEAD?
    throw AssetLoadException ("Cannot find file", filename, std::error_code ());
}

void TextureCache::store (const std::string& name, std::shared_ptr<const TextureProvider> texture) {
    this->m_textureCache.insert_or_assign (name, texture);
}
