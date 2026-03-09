#include <GL/glew.h>

#include "CWallpaper.h"
#include "RenderContext.h"

#include "WallpaperEngine/Assets/AssetLoadException.h"
#include "WallpaperEngine/Context.h"

namespace WallpaperEngine::Render {
RenderContext::RenderContext (Context& context) : m_textureCache (*context.texture_cache), m_context (context) { }

void RenderContext::render () const {
#if !NDEBUG
	const std::string str = "Rendering to output ";

	glPushDebugGroup (GL_DEBUG_SOURCE_APPLICATION, 0, -1, str.c_str ());
#endif /* DEBUG */

	// search the background in the viewport selection

	// TODO: CHECK VIEWPORT AND USE WALLPAPER'S SIZE INSTEAD
	if (this->m_wallpaper) {
		this->m_wallpaper->render ();
	}

#if !NDEBUG
	glPopDebugGroup ();
#endif /* DEBUG */
}

void RenderContext::setWallpaper (std::unique_ptr<CWallpaper> wallpaper) { this->m_wallpaper = std::move (wallpaper); }

const Context& RenderContext::getContext () const { return this->m_context; }

std::shared_ptr<const TextureProvider> RenderContext::resolveTexture (const std::string& name) const {
	try {
		return this->m_textureCache.resolve (name);
	} catch (const Assets::AssetLoadException& e) {
		// TODO: try to load from the container and store it into the cache
		throw;
	}
}

const CWallpaper& RenderContext::getWallpaper () const { return *this->m_wallpaper; }

} // namespace WallpaperEngine::Render
