#pragma once

#include <map>
#include <string>

#include "WallpaperEngine/Assets/ITexture.h"
#include "WallpaperEngine/Render/CContext.h"

using namespace WallpaperEngine::Assets;

namespace WallpaperEngine::Render
{
    class CContext;

    class CTextureCache
    {
    public:
        CTextureCache (CContext* context);
        ~CTextureCache ();

        /**
         * Checks if the given texture was already loaded and returns it
         * If the texture was not loaded yet, it tries to load it from the container
         *
         * @param filename
         * @return
         */
        const ITexture* resolve (const std::string& filename);

        /**
         * Registers a texture in the cache
         *
         * @param name
         * @param texture
         */
        void store (std::string name, const ITexture* texture);

    private:
        CContext* m_context;
        std::map<std::string, const ITexture*> m_textureCache;
    };
}
