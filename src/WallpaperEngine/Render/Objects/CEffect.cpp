#include "CEffect.h"

using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Render;

CEffect::CEffect (CImage* image, Core::Objects::CEffect* effect) :
    m_image (image),
    m_effect (effect)
{
    this->generateFBOs ();
    this->generatePasses ();
}

CImage* CEffect::getImage () const
{
    return this->m_image;
}

const std::vector<Effects::CMaterial*>& CEffect::getMaterials () const
{
    return this->m_materials;
}

CFBO* CEffect::findFBO (const std::string& name) const
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
        this->m_materials.emplace_back (new Effects::CMaterial (this, *cur));
}

void CEffect::generateFBOs ()
{
    auto cur = this->m_effect->getFbos ().begin ();
    auto end = this->m_effect->getFbos ().end ();

    for (; cur != end; cur ++)
    {
        this->m_fbos.push_back (
            new CFBO (
                (*cur)->getName (),
                ITexture::TextureFormat::ARGB8888, // TODO: CHANGE
                (*cur)->getScale (),
                this->m_image->getSize ().x / (*cur)->getScale (),
                this->m_image->getSize ().y / (*cur)->getScale (),
                this->m_image->getSize ().x / (*cur)->getScale (),
                this->m_image->getSize ().y / (*cur)->getScale ()
            )
        );
    }
}