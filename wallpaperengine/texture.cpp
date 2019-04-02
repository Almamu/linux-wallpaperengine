#include <wallpaperengine/texture.h>
#include <stdexcept>
#include <lz4.h>

#include "irrlicht.h"

namespace wp
{
    texture::texture (irr::io::path file)
    {
        this->m_texture = wp::irrlicht::driver->getTexture (file);
    }

    irr::video::ITexture* texture::getIrrTexture ()
    {
        return this->m_texture;
    }
}