#include <WallpaperEngine/texture.h>
#include <stdexcept>
#include <lz4.h>

#include <WallpaperEngine/Irrlicht/CContext.h>

extern WallpaperEngine::Irrlicht::CContext* IrrlichtContext;

namespace WallpaperEngine
{
    texture::texture (irr::io::path& file)
    {
        this->m_texture = IrrlichtContext->getDevice ()->getVideoDriver ()->getTexture (file);
    }

    irr::video::ITexture* texture::getIrrTexture ()
    {
        return this->m_texture;
    }
}