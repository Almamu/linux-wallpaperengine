#include "CMaterial.h"

using namespace WallpaperEngine::Render::Objects;

using namespace WallpaperEngine::Render::Objects::Effects;

CMaterial::CMaterial (const Render::Objects::CImage* image, const Core::Objects::Images::CMaterial* material) :
    m_image (image),
    m_material (material)
{
    // TODO: REWRITE THIS
    /*this->m_outputTexture = this->m_context->getDevice ()->getVideoDriver ()->addRenderTargetTexture (
        this->m_inputTexture->getSize (),
        (
            "_rt_WALLENGINELINUX_OUTPUT_" +
            std::to_string (this->m_image->getImage ()->getId ()) + "_" +
            std::to_string (this->m_image->getEffects ().size ()) +
            "_material_output"
        ).c_str ()
    );*/

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

// TODO: REWRITE THIS
/*
void CMaterial::OnSetConstants (irr::video::IMaterialRendererServices *services, int32_t userData)
{
    irr::s32 g_Texture0 = 0;

    irr::video::IVideoDriver* driver = services->getVideoDriver ();

    irr::core::matrix4 worldViewProj;
    worldViewProj = driver->getTransform (irr::video::ETS_PROJECTION);
    worldViewProj *= driver->getTransform (irr::video::ETS_VIEW);
    worldViewProj *= driver->getTransform (irr::video::ETS_WORLD);


    services->setPixelShaderConstant ("TextureSampler", &g_Texture0, 1);
    services->setVertexShaderConstant ("g_ModelViewProjectionMatrix", worldViewProj.pointer(), 16);
}
*/
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

    // TODO: REWRITE THIS
    /*
    uint16_t indices [] =
    {
        3, 2, 1, 0
    };

    irr::video::IVideoDriver* driver = this->getImage ()->getSceneManager ()->getVideoDriver ();

    auto mainCur = this->getPasses ().begin ();
    auto mainEnd = this->getPasses ().end ();

    for (; mainCur != mainEnd; mainCur ++)
    {
        // set the proper render target
        driver->setRenderTarget ((*mainCur)->getOutputTexture (), true, true, IntegerColor (0, 0, 0, 0));
        // set the material
        driver->setMaterial ((*mainCur)->getMaterial ());
        // draw it
        driver->drawVertexPrimitiveList (
            this->m_image->getVertex (), 4, indices, 1,
            irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT
        );
    }

    // render last pass' output into our output
    // set the proper render target
    driver->setRenderTarget (this->getOutputTexture (), true, true, IntegerColor (0, 0, 0, 0));
    // set the material
    driver->setMaterial (this->m_outputMaterial);
    // draw it
    driver->drawVertexPrimitiveList (
        this->m_image->getVertex (), 4, indices, 1,
        irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT
    );
     */
}