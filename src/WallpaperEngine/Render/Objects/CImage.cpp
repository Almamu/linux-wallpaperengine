#include "CImage.h"

#include "WallpaperEngine/Render/Shaders/Compiler.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;

CImage::CImage (CScene* scene, Core::Objects::CImage* image) :
    Render::CObject (scene, Type, image),
    m_image (image)
{
    // TODO: INITIALIZE NEEDED EFFECTS AND PROPERLY CALCULATE THESE?
    irr::f32 xright     = this->m_image->getOrigin ()->X;
    irr::f32 xleft      = -this->m_image->getOrigin ()->X;
    irr::f32 ztop       = this->m_image->getOrigin ()->Y;
    irr::f32 zbottom    = -this->m_image->getOrigin ()->Y;
    irr::f32 z          = this->getScene ()->getCamera ()->getEye ()->Z;

    // top left
    this->m_vertex [0].Pos = irr::core::vector3df (xleft,  ztop,    z);
    // top right
    this->m_vertex [1].Pos = irr::core::vector3df (xright, ztop,    z);
    // bottom right
    this->m_vertex [2].Pos = irr::core::vector3df (xright, zbottom, z);
    // bottom left
    this->m_vertex [3].Pos = irr::core::vector3df (xleft,  zbottom, z);

    this->m_vertex [0].TCoords = irr::core::vector2df (1.0f, 0.0f);
    this->m_vertex [1].TCoords = irr::core::vector2df (0.0f, 0.0f);
    this->m_vertex [2].TCoords = irr::core::vector2df (0.0f, 1.0f);
    this->m_vertex [3].TCoords = irr::core::vector2df (1.0f, 1.0f);

    this->m_vertex [0].Color = irr::video::SColor (255, 255, 255, 255);
    this->m_vertex [1].Color = irr::video::SColor (255, 255, 255, 255);
    this->m_vertex [2].Color = irr::video::SColor (255, 255, 255, 255);
    this->m_vertex [3].Color = irr::video::SColor (255, 255, 255, 255);

    this->generateMaterial ();
    this->setAutomaticCulling (irr::scene::EAC_OFF);
    this->m_boundingBox = irr::core::aabbox3d<irr::f32>(0, 0, 0, 0, 0, 0);
}

void CImage::render()
{
    uint16_t indices [] =
    {
            0, 1, 2, 3
    };

    irr::video::IVideoDriver* driver = SceneManager->getVideoDriver ();

    driver->setMaterial (this->m_material);
    driver->drawVertexPrimitiveList (
        this->m_vertex, 4, indices, 1,
        irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT
    );
}

void CImage::generateMaterial ()
{
    if (this->m_image->getMaterial ()->getPasses ()->empty () == true)
        return;

    // TODO: SUPPORT OBJECT EFFECTS AND MULTIPLE MATERIAL PASSES
    Core::Objects::Images::Materials::CPassess* pass = *this->m_image->getMaterial ()->getPasses ()->begin ();
    std::string shader = pass->getShader ();
    std::vector<std::string>* textures = pass->getTextures ();

    std::vector<std::string>::const_iterator cur = textures->begin ();
    std::vector<std::string>::const_iterator end = textures->end ();

    for (int textureNumber = 0; cur != end; cur ++, textureNumber ++)
    {
        // TODO: LOOK THIS UP PROPERLY
        irr::io::path texturepath = std::string ("materials/" + (*cur) + ".tex").c_str ();

        irr::video::ITexture* texture = this->getScene ()->getContext ()->getDevice ()->getVideoDriver ()->getTexture (texturepath);

        this->m_material.setTexture (textureNumber, texture);
    }

    // TODO: MOVE SHADER INITIALIZATION ELSEWHERE
    irr::io::path vertpath = std::string ("shaders/" + shader + ".vert").c_str ();
    irr::io::path fragpath = std::string ("shaders/" + shader + ".frag").c_str ();
    Render::Shaders::Compiler* vertshader = new Render::Shaders::Compiler (vertpath, Render::Shaders::Compiler::Type::Type_Vertex, pass->getCombos (), false);
    Render::Shaders::Compiler* fragshader = new Render::Shaders::Compiler (fragpath, Render::Shaders::Compiler::Type::Type_Pixel, pass->getCombos (), false);

    this->m_material.MaterialType = (irr::video::E_MATERIAL_TYPE)
            this->getScene ()->getContext ()->getDevice ()->getVideoDriver ()->getGPUProgrammingServices ()->addHighLevelShaderMaterial (
                vertshader->precompile ().c_str (), "main", irr::video::EVST_VS_2_0,
                fragshader->precompile ().c_str (), "main", irr::video::EPST_PS_2_0,
                this, irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL, 0, irr::video::EGSL_DEFAULT
            );

    this->m_material.MaterialType = irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL;
    this->m_material.setFlag (irr::video::EMF_LIGHTING, false);
    this->m_material.setFlag (irr::video::EMF_BLEND_OPERATION, true);
}


const irr::core::aabbox3d<irr::f32>& CImage::getBoundingBox() const
{
    return this->m_boundingBox;
}

void CImage::OnRegisterSceneNode ()
{
    SceneManager->registerNodeForRendering (this);

    ISceneNode::OnRegisterSceneNode ();
}

void CImage::OnSetConstants (irr::video::IMaterialRendererServices *services, int32_t userData)
{
    // TODO: SUPPORT SHADER PARAMETERS HERE
}

const std::string CImage::Type = "image";