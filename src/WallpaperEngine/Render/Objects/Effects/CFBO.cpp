#include "CFBO.h"

using namespace WallpaperEngine::Render::Objects::Effects;

CFBO::CFBO (Core::Objects::Effects::CFBO* fbo, const Core::Objects::CImage* image, Irrlicht::CContext* context) :
    m_fbo (fbo)
{
    irr::core::dimension2du size = irr::core::dimension2du (
        image->getSize ().X * this->getScale (),
        image->getSize ().Y * this->getScale ()
    );

    context->getDevice ()->getVideoDriver ()->addRenderTargetTexture (size, this->getName ().c_str ());
}

const irr::video::ITexture* CFBO::getTexture () const
{
    return this->m_texture;
}

const std::string& CFBO::getName () const
{
    return this->m_fbo->getName ();
}

const irr::f32& CFBO::getScale () const
{
    return this->m_fbo->getScale ();
}

const std::string& CFBO::getFormat () const
{
    return this->m_fbo->getFormat ();
}