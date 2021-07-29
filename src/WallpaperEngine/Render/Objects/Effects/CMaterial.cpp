#include "CMaterial.h"

using namespace WallpaperEngine::Render::Objects;

using namespace WallpaperEngine::Render::Objects::Effects;

CMaterial::CMaterial (Irrlicht::CContext* context, Render::Objects::CImage* image, const Core::Objects::Images::CMaterial* material, irr::video::ITexture* texture) :
    m_context (context),
    m_image (image),
    m_material (material),
    m_inputTexture (texture)
{
    this->m_outputTexture = this->m_context->getDevice ()->getVideoDriver ()->addRenderTargetTexture (
        this->m_inputTexture->getSize (),
        (
            "_rt_WALLENGINELINUX_OUTPUT_" +
            std::to_string (this->m_image->getImage ()->getId ()) + "_" +
            std::to_string (this->m_image->getEffects ().size ()) +
            "_material_output"
        ).c_str ()
    );

    this->generatePasses ();
    this->generateOutputMaterial ();
}

const std::vector<CPass*>& CMaterial::getPasses () const
{
    return this->m_passes;
}

const CImage* CMaterial::getImage () const
{
    return this->m_image;
}

irr::video::ITexture* CMaterial::getOutputTexture () const
{
    return this->m_outputTexture;
}

irr::video::ITexture* CMaterial::getInputTexture () const
{
    return this->m_inputTexture;
}

void CMaterial::generatePasses ()
{
    auto cur = this->m_material->getPasses ().begin ();
    auto end = this->m_material->getPasses ().end ();
    irr::video::ITexture* inputTexture = this->getInputTexture ();

    for (; cur != end; cur ++)
    {
        CPass* pass = new CPass (this->m_context, this, *cur, inputTexture);

        inputTexture = pass->getOutputTexture ();

        this->m_passes.push_back (pass);
    }

    this->m_outputMaterial.setTexture (0, inputTexture);
}

void CMaterial::generateOutputMaterial ()
{
    this->m_outputMaterial.MaterialType = irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL;
    this->m_outputMaterial.setFlag (irr::video::EMF_LIGHTING, false);
    this->m_outputMaterial.setFlag (irr::video::EMF_BLEND_OPERATION, true);
    this->m_outputMaterial.Wireframe = false;
    this->m_outputMaterial.Lighting = false;
}

void CMaterial::render ()
{
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
        driver->setRenderTarget ((*mainCur)->getOutputTexture (), true, true, irr::video::SColor (0, 0, 0, 0));
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
    driver->setRenderTarget (this->getOutputTexture (), true, true, irr::video::SColor (0, 0, 0, 0));
    // set the material
    driver->setMaterial (this->m_outputMaterial);
    // draw it
    driver->drawVertexPrimitiveList (
        this->m_image->getVertex (), 4, indices, 1,
        irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT
    );
}