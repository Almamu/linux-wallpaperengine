#pragma once

#include <memory>
#include <optional>

#include "TextureCache.h"
namespace WallpaperEngine {
namespace Media {
    class MediaSource;
}
namespace Assets {
    class AssetLocator;
}
class Context;
namespace Render {
    class AlbumTexture;
    class TextureCache;

    class RenderContext {
    public:
	explicit RenderContext (Context& context, Assets::AssetLocator& locator);

	[[nodiscard]] const Context& getContext () const;
	[[nodiscard]] std::shared_ptr<const TextureProvider> resolveTexture (const std::string& name);
	[[nodiscard]] const Assets::AssetLocator& getAssetLocator () const;

	void albumArtUrlChange (std::optional<std::string>& url);

    private:
	/** The previous album thumbnail texture */
	std::shared_ptr<const AlbumTexture> m_previousThumbnail = nullptr;
	/** The current album thumbnail texture */
	std::shared_ptr<const AlbumTexture> m_currentThumbnail = nullptr;
	/** Texture cache for the render */
	TextureCache& m_textureCache;
	/** The context that owns this render context */
	Context& m_context;
	/** The asset locator used for this render context */
	Assets::AssetLocator& m_locator;
    };
} // namespace Render
} // namespace WallpaperEngine
