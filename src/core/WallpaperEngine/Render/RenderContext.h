#pragma once

#include <memory>

#include "TextureCache.h"
namespace WallpaperEngine {
namespace Assets {
	class AssetLocator;
}
class Context;
namespace Render {
	class TextureCache;

	class RenderContext {
	public:
		explicit RenderContext (Context& context, Assets::AssetLocator& locator);

		[[nodiscard]] const Context& getContext () const;
		[[nodiscard]] std::shared_ptr<const TextureProvider> resolveTexture (const std::string& name);
		[[nodiscard]] const Assets::AssetLocator& getAssetLocator () const;

	private:
		/** Texture cache for the render */
		TextureCache& m_textureCache;
		/** The context that owns this render context */
		Context& m_context;
		/** The asset locator used for this render context */
		Assets::AssetLocator& m_locator;
	};
} // namespace Render
} // namespace WallpaperEngine
