#pragma once

#include <map>
#include <string>
#include <memory>

#include "WallpaperEngine/Assets/ITexture.h"
#include "WallpaperEngine/Render/CRenderContext.h"
#include "WallpaperEngine/Render/Helpers/CContextAware.h"

using namespace WallpaperEngine::Assets;

namespace WallpaperEngine::Render {
namespace Helpers {
class CContextAware;
}

class CRenderContext;

class CTextureCache final : Helpers::CContextAware {
  public:
    explicit CTextureCache (CRenderContext& context);

    /**
     * Checks if the given texture was already loaded and returns it
     * If the texture was not loaded yet, it tries to load it from the container
     *
     * @param filename
     * @return
     */
    std::shared_ptr<const ITexture> resolve (const std::string& filename);

    /**
     * Registers a texture in the cache
     *
     * @param name
     * @param texture
     */
    void store (const std::string& name, std::shared_ptr<const ITexture> texture);

  private:
    /** Cached textures */
    std::map<std::string, std::shared_ptr<const ITexture>> m_textureCache = {};
};
} // namespace WallpaperEngine::Render
