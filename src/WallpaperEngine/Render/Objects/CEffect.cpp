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
            "_effect_output"
        ).c_str ()
    );

    this->generateFBOs ();
    this->generatePasses ();
    this->generateOutputMaterial ();
}

irr::video::ITexture* CEffect::getInputTexture () const
{
    return this->m_inputTexture;
}

irr::video::ITexture *const CEffect::getOutputTexture () const
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
    irr::video::ITexture* inputTexture = this->getInputTexture ();

    for (; cur != end; cur ++)
    {
        Effects::CMaterial* material = new Effects::CMaterial (this->m_context, this->m_image, *cur, inputTexture);

        // next input texture will be the output texture of the material
        inputTexture = material->getOutputTexture ();

        this->m_materials.push_back (material);
    }

    this->m_outputMaterial.setTexture (0, inputTexture);
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

void CEffect::generateOutputMaterial ()
{
    this->m_outputMaterial.MaterialType = irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL;
    this->m_outputMaterial.setFlag (irr::video::EMF_LIGHTING, false);
    this->m_outputMaterial.setFlag (irr::video::EMF_BLEND_OPERATION, true);
    this->m_outputMaterial.Wireframe = false;
    this->m_outputMaterial.Lighting = false;
}

void CEffect::render ()
{
    uint16_t indices [] =
    {
        3, 2, 1, 0
    };

    irr::video::IVideoDriver* driver = this->getImage ()->getSceneManager ()->getVideoDriver ();

    auto mainCur = this->getMaterials ().begin ();
    auto mainEnd = this->getMaterials ().end ();

    for (; mainCur != mainEnd; mainCur ++)
    {
        (*mainCur)->render ();
    }

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