#include "CEffect.h"

using namespace WallpaperEngine::Render::Objects;

CEffect::CEffect (CImage* image, Core::Objects::CEffect* effect) :
    m_image (image),
    m_effect (effect)
{
    this->generateFBOs ();
    this->generatePasses ();
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
        this->m_materials.emplace_back (new Effects::CMaterial (this->getImage (), *cur));
}

void CEffect::generateFBOs ()
{
    auto cur = this->m_effect->getFbos ().begin ();
    auto end = this->m_effect->getFbos ().end ();

    for (; cur != end; cur ++)
    {
        this->m_fbos.push_back (
            new Effects::CFBO (*cur, this->m_image->getImage ())
        );
    }
}

void CEffect::render (GLuint drawTo, GLuint inputTexture)
{
    auto begin = this->getMaterials ().begin ();
    auto cur = this->getMaterials ().begin ();
    auto end = this->getMaterials ().end ();

    for (; cur != end; cur ++)
    {
        // pingpong buffer only if not the first pass (as it would be taken care by the CImage)
        if (cur != begin)
            this->getImage ()->getScene ()->pinpongFramebuffer (&drawTo, &inputTexture);

        (*cur)->render (drawTo, inputTexture);
    }
}