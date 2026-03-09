#pragma once

#include <memory>

#include "TextureCache.h"
namespace WallpaperEngine {
struct Context;
namespace Application {
	class WallpaperApplication;
}

namespace Render {
	class CWallpaper;
	class TextureCache;

	class RenderContext {
	public:
		explicit RenderContext (Context& context);

		void render () const;
		void setWallpaper (std::unique_ptr<CWallpaper> wallpaper);
		[[nodiscard]] const Context& getContext () const;
		[[nodiscard]] std::shared_ptr<const TextureProvider> resolveTexture (const std::string& name) const;
		[[nodiscard]] const CWallpaper& getWallpaper () const;

	private:
		/** Wallpaper being rendered by this context */
		std::unique_ptr<CWallpaper> m_wallpaper;
		/** Texture cache for the render */
		TextureCache& m_textureCache;
		/** The context that owns this render context */
		Context& m_context;
	};
} // namespace Render
} // namespace WallpaperEngine
