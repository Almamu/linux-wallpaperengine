#include "CImage.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;

extern irr::f32 g_Time;

CImage::CImage (CScene* scene, Core::Objects::CImage* image) :
    Render::CObject (scene, Type, image),
    m_image (image)
{
    // TODO: INITIALIZE NEEDED EFFECTS AND PROPERLY CALCULATE THESE?
    irr::f32 xsize = this->m_image->getSize ().X;
    irr::f32 ysize = this->m_image->getSize ().Y;
    irr::f32 xscale = this->m_image->getScale ().X;
    irr::f32 yscale = this->m_image->getScale ().Y;

    if (xsize == 0.0f || ysize == 0.0f)
    {
        xsize = 1920.0f;
        ysize = 1080.0f;

        this->getScene ()->getContext ()->getDevice ()->getLogger ()->log (
            "Initializing xsise and ysize as default values 1920 and 1080",
            this->getImage ()->getName ().c_str ()
        );
    }

    irr::core::vector3df cameraCenter = this->getScene ()->getCamera ()->getCenter ();

    // take the orthogonal projection into account
    irr::f32 scene_width = this->getScene ()->getScene ()->getOrthogonalProjection ()->getWidth ();
    irr::f32 scene_height = this->getScene ()->getScene ()->getOrthogonalProjection ()->getHeight ();

    irr::f32 xright     = (-scene_width  / 2.0f + this->m_image->getOrigin ().X + xsize * xscale / 2.0f) + cameraCenter.X;
    irr::f32 xleft      = (-scene_width  / 2.0f + this->m_image->getOrigin ().X - xsize * xscale / 2.0f) + cameraCenter.X;
    irr::f32 ytop       = (-scene_height / 2.0f + this->m_image->getOrigin ().Y + ysize * yscale / 2.0f) + cameraCenter.Y;
    irr::f32 ybottom    = (-scene_height / 2.0f + this->m_image->getOrigin ().Y - ysize * yscale / 2.0f) + cameraCenter.Y;
    irr::f32 z          = this->m_image->getOrigin ().Z;

    // top left
    this->m_vertex [0].Pos = irr::core::vector3df (xleft,  ytop,    z);
    // top right
    this->m_vertex [1].Pos = irr::core::vector3df (xright, ytop,    z);
    // bottom right
    this->m_vertex [2].Pos = irr::core::vector3df (xright, ybottom, z);
    // bottom left
    this->m_vertex [3].Pos = irr::core::vector3df (xleft,  ybottom, z);

    this->m_vertex [0].TCoords = irr::core::vector2df (1.0f, 0.0f);
    this->m_vertex [1].TCoords = irr::core::vector2df (0.0f, 0.0f);
    this->m_vertex [2].TCoords = irr::core::vector2df (0.0f, 1.0f);
    this->m_vertex [3].TCoords = irr::core::vector2df (1.0f, 1.0f);

    this->m_vertex [0].Color = irr::video::SColor (255, 255, 255, 255);
    this->m_vertex [1].Color = irr::video::SColor (255, 255, 255, 255);
    this->m_vertex [2].Color = irr::video::SColor (255, 255, 255, 255);
    this->m_vertex [3].Color = irr::video::SColor (255, 255, 255, 255);

    this->setAutomaticCulling (irr::scene::EAC_OFF);
    this->m_boundingBox = irr::core::aabbox3d<irr::f32>(0, 0, 0, 0, 0, 0);

    // load the texture in the main material
    irr::io::path texturepath = this->getScene ()->getContext ()->resolveMaterials (
        (*(*this->m_image->getMaterial ()->getPasses ().begin ())->getTextures ().begin ())
    );
    irr::video::ITexture *texture = this->getScene ()->getContext ()->getDevice ()->getVideoDriver ()->getTexture (texturepath);

    this->m_material = new Render::Objects::Effects::CMaterial (this->getScene ()->getContext (), this, this->m_image->getMaterial (), texture);

    auto effectsCur = this->m_image->getEffects ().begin ();
    auto effectsEnd = this->m_image->getEffects ().end ();

    texture = this->m_material->getOutputTexture ();

    for (; effectsCur != effectsEnd; effectsCur ++)
    {
        CEffect* effect = new CEffect (this, *effectsCur, this->getScene ()->getContext (), texture);

        this->m_effects.push_back (effect);

        texture = effect->getOutputTexture ();
    }

    this->generateMaterial (texture);
}

void CImage::render()
{
    uint16_t indices [] =
    {
        3, 2, 1, 0
    };

    irr::video::IVideoDriver* driver = SceneManager->getVideoDriver ();

    // first render the base material
    this->m_material->render ();

    // now render all the effects, they should already be linked to each other
    auto effectCur = this->m_effects.begin ();
    auto effectEnd = this->m_effects.end ();

    for (; effectCur != effectEnd; effectCur ++)
    {
        (*effectCur)->render ();
    }

    // set render target to the screen
    driver->setRenderTarget (irr::video::ERT_FRAME_BUFFER, false, false);
    // set the material to use
    driver->setMaterial (this->m_irrlichtMaterial);
    // draw it
    driver->drawVertexPrimitiveList (
        this->m_vertex, 4, indices, 1,
        irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT
    );
}

void CImage::generateMaterial (irr::video::ITexture* resultTexture)
{
    this->m_irrlichtMaterial.setTexture (0, resultTexture);
    this->m_irrlichtMaterial.MaterialType = irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL;
    this->m_irrlichtMaterial.setFlag (irr::video::EMF_LIGHTING, false);
    this->m_irrlichtMaterial.setFlag (irr::video::EMF_BLEND_OPERATION, true);
    this->m_irrlichtMaterial.Wireframe = false;
    this->m_irrlichtMaterial.Lighting = false;
}

const irr::core::aabbox3d<irr::f32>& CImage::getBoundingBox() const
{
    return this->m_boundingBox;
}

const Core::Objects::CImage* CImage::getImage () const
{
    return this->m_image;
}

const std::vector<CEffect*>& CImage::getEffects () const
{
    return this->m_effects;
}

const irr::video::S3DVertex* CImage::getVertex () const
{
    return this->m_vertex;
}

const std::string CImage::Type = "image";