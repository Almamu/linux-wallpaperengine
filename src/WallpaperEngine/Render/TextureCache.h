#pragma once

#include <map>
#include <memory>
#include <string>

#include "TextureProvider.h"
#include "WallpaperEngine/Render/Helpers/ContextAware.h"
#include "WallpaperEngine/Render/RenderContext.h"

using namespace WallpaperEngine::Render;

namespace WallpaperEngine::Render {
namespace Helpers {
    class ContextAware;
}

class RenderContext;

class TextureCache final : Helpers::ContextAware {
public:
    explicit TextureCache (RenderContext& context);

    /**
     * Checks if the given texture was already loaded and returns it
     * If the texture was not loaded yet, it tries to load it from the container
     *
     * @param filename
     * @return
     */
    std::shared_ptr<const TextureProvider> resolve (const std::string& filename);

    /**
     * Registers a texture in the cache
     *
     * @param name
     * @param texture
     */
    void store (const std::string& name, std::shared_ptr<const TextureProvider> texture);

    /**
     * Runs a texture update cycle on everything registered in the cache
     */
    void update () const;

private:
    /** Cached textures */
    std::map<std::string, std::shared_ptr<const TextureProvider>> m_textureCache = {};
};
} // namespace WallpaperEngine::Render
