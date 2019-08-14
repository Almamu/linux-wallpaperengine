#include <WallpaperEngine/texture.h>
#include <stdexcept>
#include <lz4.h>

#include "irrlicht.h"

namespace WallpaperEngine
{
    texture::texture (irr::io::path& file)
    {
        this->m_texture = WallpaperEngine::irrlicht::driver->getTexture (file);
    }

    irr::video::ITexture* texture::getIrrTexture ()
    {
        return this->m_texture;
    }
}