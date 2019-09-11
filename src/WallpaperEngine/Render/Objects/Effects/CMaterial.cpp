#include "CMaterial.h"

using namespace WallpaperEngine::Render::Objects;

using namespace WallpaperEngine::Render::Objects::Effects;

CMaterial::CMaterial (Irrlicht::CContext* context, Render::Objects::CEffect* effect, Core::Objects::Images::CMaterial* material) :
    m_context (context),
    m_effect (effect),
    m_material (material)
{
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

    for (; cur != end; cur ++)
    {
        this->m_passes.push_back (
            new CPass (this->m_context, this, *cur)
        );
    }
}