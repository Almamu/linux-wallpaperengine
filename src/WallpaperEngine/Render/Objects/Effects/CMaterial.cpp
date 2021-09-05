#include "CMaterial.h"

using namespace WallpaperEngine::Render::Objects;

using namespace WallpaperEngine::Render::Objects::Effects;

CMaterial::CMaterial (const Render::Objects::CImage* image, const Core::Objects::Images::CMaterial* material) :
    m_image (image),
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
    return this->m_image;
}

void CMaterial::generatePasses ()
{
    auto cur = this->m_material->getPasses ().begin ();
    auto end = this->m_material->getPasses ().end ();

    // these are simple now, just create the entries and done
    for (; cur != end; cur ++)
        this->m_passes.emplace_back (new CPass (this, *cur));
}

void CMaterial::render (GLuint drawTo, GLuint inputTexture)
{
    // get the orthogonal projection
    auto projection = this->getImage ()->getScene ()->getScene ()->getOrthogonalProjection ();

    auto begin = this->getPasses ().begin ();
    auto cur = this->getPasses ().begin ();
    auto end = this->getPasses ().end ();

    for (; cur != end; cur ++)
    {
        // get the current framebuffer to use (only after the first iteration)
        if (cur != begin)
            this->getImage ()->getScene ()->pinpongFramebuffer (&drawTo, &inputTexture);

        // bind to this new framebuffer
        glBindFramebuffer (GL_FRAMEBUFFER, drawTo);
        // set the viewport, for now use the scene width/height but we might want to use image's size TODO: INVESTIGATE THAT
        glViewport (0, 0, projection->getWidth (), projection->getHeight ());
        // render the pass
        (*cur)->render (drawTo, inputTexture);
    }
}