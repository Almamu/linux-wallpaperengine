#include "CMaterial.h"

using namespace WallpaperEngine::Render::Objects;

using namespace WallpaperEngine::Render::Objects::Effects;

CMaterial::CMaterial (const Render::Objects::CEffect* effect, const Core::Objects::Images::CMaterial* material) :
    m_effect (effect),
    m_material (material)
{
    this->generatePasses ();
}

const std::vector<CPass*>& CMaterial::getPasses () const
{
    return this->m_passes;
}

CImage* CMaterial::getImage () const
{
    return this->m_effect->getImage ();
}

void CMaterial::generatePasses ()
{
    // these are simple now, just create the entries and done
    for (const auto& cur : this->m_material->getPasses ())
        this->m_passes.emplace_back (new CPass (this, cur));
}

const Core::Objects::Images::CMaterial* CMaterial::getMaterial () const
{
    return this->m_material;
}

const CEffect* CMaterial::getEffect () const
{
    return this->m_effect;
}