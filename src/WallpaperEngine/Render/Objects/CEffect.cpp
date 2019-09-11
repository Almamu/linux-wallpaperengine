#include "CEffect.h"

using namespace WallpaperEngine::Render::Objects;

CEffect::CEffect (CImage* image, Core::Objects::CEffect* effect, Irrlicht::CContext* context, irr::video::ITexture* input) :
    m_context (context),
    m_image (image),
    m_effect (effect),
    m_inputTexture (input)
{
    irr::core::dimension2du size = irr::core::dimension2du (
        this->m_image->getImage ()->getSize ().X,
        this->m_image->getImage ()->getSize ().Y
    );

    this->m_outputTexture = this->m_context->getDevice ()->getVideoDriver ()->addRenderTargetTexture (
        size,
        (
            "_rt_WALLENGINELINUX_OUTPUT_" +
            std::to_string (image->getImage ()->getId ()) + "_" +
            std::to_string (this->m_image->getEffects ().size ()) +
            "_output"
        ).c_str ()
    );

    this->generateFBOs ();
    this->generatePasses ();
}

const irr::video::ITexture* CEffect::getInputTexture () const
{
    return this->m_inputTexture;
}

const irr::video::ITexture* CEffect::getOutputTexture () const
{
    return this->m_outputTexture;
}

const CImage* CEffect::getImage () const
{
    return this->m_image;
}

const std::vector<Effects::CMaterial*>& CEffect::getMaterials () const
{
    return this->m_materials;
}

Effects::CFBO* CEffect::findFBO (const std::string& name)
{
    auto cur = this->m_fbos.begin ();
    auto end = this->m_fbos.end ();

    for (; cur != end; cur ++)
    {
        if ((*cur)->getName () == name)
        {
            return *cur;
        }
    }

    return nullptr;
}

void CEffect::generatePasses ()
{
    auto cur = this->m_effect->getMaterials ().begin ();
    auto end = this->m_effect->getMaterials ().end ();

    for (; cur != end; cur ++)
    {
        this->m_materials.push_back (
            new Effects::CMaterial (this->m_context, this, *cur)
        );
    }
}

void CEffect::generateFBOs ()
{
    auto cur = this->m_effect->getFbos ().begin ();
    auto end = this->m_effect->getFbos ().end ();

    for (; cur != end; cur ++)
    {
        this->m_fbos.push_back (
            new Effects::CFBO (*cur, this->m_image->getImage (), this->m_context)
        );
    }
}