#include <WallpaperEngine/texture.h>
#include <stdexcept>
#include <lz4.h>

#include "WallpaperEngine/Irrlicht/Irrlicht.h"

namespace WallpaperEngine
{
    texture::texture (irr::io::path& file)
    {
        this->m_texture = WallpaperEngine::Irrlicht::driver->getTexture (file);
    }

    irr::video::ITexture* texture::getIrrTexture ()
    {
        return this->m_texture;
    }
}