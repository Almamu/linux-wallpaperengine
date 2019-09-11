#include "CMaterial.h"

using namespace WallpaperEngine::Render::Objects;

using namespace WallpaperEngine::Render::Objects::Effects;

CMaterial::CMaterial (Irrlicht::CContext* context, Render::Objects::CEffect* effect, Core::Objects::Images::CMaterial* material, const irr::video::ITexture* texture) :
    m_context (context),
    m_effect (effect),
    m_material (material),
    m_inputTexture (texture)
{
    this->m_outputTexture = this->m_context->getDevice ()->getVideoDriver ()->addRenderTargetTexture (
        this->m_inputTexture->getSize (),
        (
            "_rt_WALLENGINELINUX_OUTPUT_" +
            std::to_string (this->m_effect->getImage ()->getImage ()->getId ()) + "_" +
            std::to_string (this->m_effect->getImage ()->getEffects ().size ()) +
            "_material_output"
        ).c_str ()
    );

    this->generatePasses ();
}

const std::vector<CPass*>& CMaterial::getPasses () const
{
    return this->m_passes;
}

const CImage* CMaterial::getImage () const
{
    return this->m_effect->getImage ();
}

const irr::video::ITexture* CMaterial::getOutputTexture () const
{
    return this->m_outputTexture;
}

const irr::video::ITexture* CMaterial::getInputTexture () const
{
    return this->m_inputTexture;
}

void CMaterial::generatePasses ()
{
    auto cur = this->m_material->getPasses ().begin ();
    auto end = this->m_material->getPasses ().end ();
    const irr::video::ITexture* inputTexture = this->getInputTexture ();

    for (; cur != end; cur ++)
    {
        CPass* pass = new CPass (this->m_context, this, *cur, inputTexture);

        inputTexture = pass->getOutputTexture ();

        this->m_passes.push_back (pass);
    }
}