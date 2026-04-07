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

TextureCache::TextureCache () { }

std::shared_ptr<const TextureProvider> TextureCache::resolve (const std::string& filename) {
	if (const auto found = this->m_textureCache.find (filename); found != this->m_textureCache.end ()) {
		return found->second;
	}

	throw Assets::AssetLoadException (
		"Cannot find file", filename, std::make_error_code (std::errc::no_such_file_or_directory)
	);
}

void TextureCache::store (const std::string& name, std::shared_ptr<const TextureProvider> texture) {
	this->m_textureCache.insert_or_assign (name, texture);
}
