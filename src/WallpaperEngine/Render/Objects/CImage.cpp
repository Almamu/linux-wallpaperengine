#include "CImage.h"

#include "WallpaperEngine/Render/Shaders/Compiler.h"
#include "WallpaperEngine/Render/Shaders/Parameters/CShaderParameter.h"
#include "WallpaperEngine/Render/Shaders/Parameters/CShaderParameterFloat.h"
#include "WallpaperEngine/Render/Shaders/Parameters/CShaderParameterInteger.h"
#include "WallpaperEngine/Render/Shaders/Parameters/CShaderParameterVector2.h"
#include "WallpaperEngine/Render/Shaders/Parameters/CShaderParameterVector3.h"
#include "WallpaperEngine/Render/Shaders/Parameters/CShaderParameterVector4.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Render::Shaders::Parameters;

extern irr::f32 g_Time;

CImage::CImage (CScene* scene, Core::Objects::CImage* image) :
    Render::CObject (scene, Type, image),
    m_image (image),
    m_passes (0)
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

    std::vector<irr::video::SMaterial>::const_iterator cur = this->m_materials.begin ();
    std::vector<irr::video::SMaterial>::const_iterator end = this->m_materials.end ();

    std::vector<irr::video::ITexture*>::const_iterator textureCur = this->m_renderTextures.begin ();
    std::vector<irr::video::ITexture*>::const_iterator textureEnd = this->m_renderTextures.end ();

    for (; cur != end; cur ++)
    {
        if (textureCur == textureEnd)
        {
            driver->setRenderTarget (0, false, false);
        }
        else
        {
            driver->setRenderTarget (*textureCur, true, true, irr::video::SColor (0, 0, 0, 0));
            textureCur ++;
        }

        driver->setMaterial (*cur);
        driver->drawVertexPrimitiveList (
            this->m_vertex, 4, indices, 1,
            irr::video::EVT_STANDARD, irr::scene::EPT_QUADS, irr::video::EIT_16BIT
        );
    }
}

void CImage::generateMaterial ()
{
    if (this->m_image->getMaterial ()->getPasses ()->empty () == true)
        return;

    std::vector<Core::Objects::Images::Materials::CPassess*>::const_iterator cur = this->m_image->getMaterial ()->getPasses ()->begin ();
    std::vector<Core::Objects::Images::Materials::CPassess*>::const_iterator end = this->m_image->getMaterial ()->getPasses ()->end ();

    for (; cur != end; cur ++, this->m_passes ++)
    {
        this->generatePass (*cur);
    }

    std::vector<Core::Objects::CEffect*>::const_iterator effectCur = this->m_image->getEffects ()->begin ();
    std::vector<Core::Objects::CEffect*>::const_iterator effectEnd = this->m_image->getEffects ()->end ();

    for (; effectCur != effectEnd; effectCur ++)
    {
        std::vector<Core::Objects::Images::CMaterial*>::const_iterator materialCur = (*effectCur)->getMaterials ()->begin ();
        std::vector<Core::Objects::Images::CMaterial*>::const_iterator materialEnd = (*effectCur)->getMaterials ()->end ();

        for (; materialCur != materialEnd; materialCur ++)
        {
            cur = (*materialCur)->getPasses ()->begin ();
            end = (*materialCur)->getPasses ()->end ();

            for (; cur != end; cur ++, this->m_passes ++)
            {
                this->generatePass (*cur);
            }
        }
    }
}

void CImage::generatePass (Core::Objects::Images::Materials::CPassess* pass)
{
    std::vector<std::string>* textures = pass->getTextures ();
    irr::video::SMaterial material;

    std::vector<std::string>::const_iterator texturesCur = textures->begin ();
    std::vector<std::string>::const_iterator texturesEnd = textures->end ();

    for (int textureNumber = 0; texturesCur != texturesEnd; texturesCur ++, textureNumber ++)
    {
        // TODO: LOOK THIS UP PROPERLY
        irr::io::path texturepath = std::string ("materials/" + (*texturesCur) + ".tex").c_str ();
        irr::video::ITexture* texture = nullptr;

        if (textureNumber == 0 && this->m_passes > 0)
        {
            texture = this->getScene ()->getContext ()->getDevice ()->getVideoDriver ()->addRenderTargetTexture (
                irr::core::dimension2d<irr::u32> (
                    this->getScene ()->getScene ()->getOrthogonalProjection()->getWidth (),
                    this->getScene ()->getScene ()->getOrthogonalProjection()->getHeight ()
                ), ("_RT_" + this->m_image->getName ()).c_str ()
            );

            this->m_renderTextures.push_back (texture);
        }
        else
        {
            texture = this->getScene ()->getContext ()->getDevice ()->getVideoDriver ()->getTexture (texturepath);
        }

        material.setTexture (textureNumber, texture);
    }

    // TODO: MOVE SHADER INITIALIZATION ELSEWHERE
    irr::io::path vertpath = std::string ("shaders/" + pass->getShader () + ".vert").c_str ();
    irr::io::path fragpath = std::string ("shaders/" + pass->getShader () + ".frag").c_str ();
    Render::Shaders::Compiler* vertexShader = new Render::Shaders::Compiler (vertpath, Render::Shaders::Compiler::Type::Type_Vertex, pass->getCombos (), false);
    Render::Shaders::Compiler* pixelShader = new Render::Shaders::Compiler (fragpath, Render::Shaders::Compiler::Type::Type_Pixel, pass->getCombos (), false);

    material.MaterialType = (irr::video::E_MATERIAL_TYPE)
        this->getScene ()->getContext ()->getDevice ()->getVideoDriver ()->getGPUProgrammingServices ()->addHighLevelShaderMaterial (
            vertexShader->precompile ().c_str (), "main", irr::video::EVST_VS_2_0,
            pixelShader->precompile ().c_str (), "main", irr::video::EPST_PS_2_0,
            this, irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL, this->m_passes, irr::video::EGSL_DEFAULT
        );

    material.setFlag (irr::video::EMF_LIGHTING, false);
    material.setFlag (irr::video::EMF_BLEND_OPERATION, true);

    this->m_vertexShaders.push_back (vertexShader);
    this->m_pixelShaders.push_back (pixelShader);
    this->m_materials.push_back (material);
}

const irr::core::aabbox3d<irr::f32>& CImage::getBoundingBox() const
{
    return this->m_boundingBox;
}

void CImage::OnRegisterSceneNode ()
{
    if (this->m_image->isVisible () == true)
        SceneManager->registerNodeForRendering (this);

    ISceneNode::OnRegisterSceneNode ();
}

void CImage::OnSetConstants (irr::video::IMaterialRendererServices *services, int32_t userData)
{
    irr::f32 g_Texture0 = 0;
    irr::f32 g_Texture1 = 1;
    irr::f32 g_Texture2 = 2;
    irr::f32 g_Texture3 = 3;
    irr::f32 g_Texture4 = 4;
    irr::f32 g_Texture5 = 5;
    irr::f32 g_Texture6 = 6;
    irr::f32 g_Texture7 = 7;

    irr::f32 g_Texture0Rotation [4] = { this->m_image->getAngles ()->X, this->m_image->getAngles ()->Y, this->m_image->getAngles ()->Z, this->m_image->getAngles ()->Z};
    irr::f32 g_Texture1Rotation [4] = { this->m_image->getAngles ()->X, this->m_image->getAngles ()->Y, this->m_image->getAngles ()->Z, this->m_image->getAngles ()->Z};
    irr::f32 g_Texture2Rotation [4] = { this->m_image->getAngles ()->X, this->m_image->getAngles ()->Y, this->m_image->getAngles ()->Z, this->m_image->getAngles ()->Z};
    irr::f32 g_Texture3Rotation [4] = { this->m_image->getAngles ()->X, this->m_image->getAngles ()->Y, this->m_image->getAngles ()->Z, this->m_image->getAngles ()->Z};
    irr::f32 g_Texture4Rotation [4] = { this->m_image->getAngles ()->X, this->m_image->getAngles ()->Y, this->m_image->getAngles ()->Z, this->m_image->getAngles ()->Z};
    irr::f32 g_Texture5Rotation [4] = { this->m_image->getAngles ()->X, this->m_image->getAngles ()->Y, this->m_image->getAngles ()->Z, this->m_image->getAngles ()->Z};
    irr::f32 g_Texture6Rotation [4] = { this->m_image->getAngles ()->X, this->m_image->getAngles ()->Y, this->m_image->getAngles ()->Z, this->m_image->getAngles ()->Z};
    irr::f32 g_Texture7Rotation [4] = { this->m_image->getAngles ()->X, this->m_image->getAngles ()->Y, this->m_image->getAngles ()->Z, this->m_image->getAngles ()->Z};

    irr::f32 g_Texture0Resolution [4] = { this->m_image->getSize ()->X, this->m_image->getSize ()->Y, this->m_image->getSize ()->X, this->m_image->getSize ()->Y};
    irr::f32 g_Texture1Resolution [4] = { this->m_image->getSize ()->X, this->m_image->getSize ()->Y, this->m_image->getSize ()->X, this->m_image->getSize ()->Y};
    irr::f32 g_Texture2Resolution [4] = { this->m_image->getSize ()->X, this->m_image->getSize ()->Y, this->m_image->getSize ()->X, this->m_image->getSize ()->Y};
    irr::f32 g_Texture3Resolution [4] = { this->m_image->getSize ()->X, this->m_image->getSize ()->Y, this->m_image->getSize ()->X, this->m_image->getSize ()->Y};
    irr::f32 g_Texture4Resolution [4] = { this->m_image->getSize ()->X, this->m_image->getSize ()->Y, this->m_image->getSize ()->X, this->m_image->getSize ()->Y};
    irr::f32 g_Texture5Resolution [4] = { this->m_image->getSize ()->X, this->m_image->getSize ()->Y, this->m_image->getSize ()->X, this->m_image->getSize ()->Y};
    irr::f32 g_Texture6Resolution [4] = { this->m_image->getSize ()->X, this->m_image->getSize ()->Y, this->m_image->getSize ()->X, this->m_image->getSize ()->Y};
    irr::f32 g_Texture7Resolution [4] = { this->m_image->getSize ()->X, this->m_image->getSize ()->Y, this->m_image->getSize ()->X, this->m_image->getSize ()->Y};

    irr::video::IVideoDriver* driver = services->getVideoDriver ();

    irr::core::matrix4 worldViewProj;worldViewProj.makeIdentity();
    worldViewProj = driver->getTransform(irr::video::ETS_PROJECTION);
    worldViewProj *= driver->getTransform(irr::video::ETS_VIEW);
    worldViewProj *= driver->getTransform(irr::video::ETS_WORLD);

    Render::Shaders::Compiler* vertexShader = this->m_vertexShaders.at (userData);
    Render::Shaders::Compiler* pixelShader = this->m_pixelShaders.at (userData);

    std::vector<Render::Shaders::Parameters::CShaderParameter*>::const_iterator cur = vertexShader->getParameters ().begin ();
    std::vector<Render::Shaders::Parameters::CShaderParameter*>::const_iterator end = vertexShader->getParameters ().end ();

    for (; cur != end; cur ++)
    {
        if ((*cur)->Is <CShaderParameterInteger> () == true)
        {
            services->setVertexShaderConstant (
                (*cur)->getName ().c_str (),
                (irr::s32*) (*cur)->getValue (),
                (*cur)->getSize ()
            );
        }
        else if (
            (*cur)->Is <CShaderParameterFloat> () == true ||
            (*cur)->Is <CShaderParameterVector2> () == true ||
            (*cur)->Is <CShaderParameterVector3> () == true ||
            (*cur)->Is <CShaderParameterVector4> () == true)
        {
            services->setVertexShaderConstant (
                (*cur)->getName ().c_str (),
                (irr::f32*) (*cur)->getValue (),
                (*cur)->getSize ()
            );
        }
    }

    cur = pixelShader->getParameters ().begin ();
    end = pixelShader->getParameters ().end ();

    for (; cur != end; cur ++)
    {
        if ((*cur)->Is <CShaderParameterInteger> () == true)
        {
            services->setPixelShaderConstant (
                (*cur)->getName ().c_str (),
                (irr::s32*) (*cur)->getValue (),
                (*cur)->getSize ()
            );
        }
        else if (
            (*cur)->Is <CShaderParameterFloat> () == true ||
            (*cur)->Is <CShaderParameterVector2> () == true ||
            (*cur)->Is <CShaderParameterVector3> () == true ||
            (*cur)->Is <CShaderParameterVector4> () == true)
        {
            services->setPixelShaderConstant (
                (*cur)->getName ().c_str (),
                (irr::f32*) (*cur)->getValue (),
                (*cur)->getSize ()
            );
        }
    }

    services->setVertexShaderConstant ("g_Time", &g_Time, 1);
    services->setPixelShaderConstant  ("g_Time", &g_Time, 1);

    services->setVertexShaderConstant ("g_ModelViewProjectionMatrix", worldViewProj.pointer(), 16);

    services->setVertexShaderConstant ("g_Texture0Resolution", g_Texture0Resolution, 4);
    services->setVertexShaderConstant ("g_Texture1Resolution", g_Texture1Resolution, 4);
    services->setVertexShaderConstant ("g_Texture2Resolution", g_Texture2Resolution, 4);
    services->setVertexShaderConstant ("g_Texture3Resolution", g_Texture3Resolution, 4);
    services->setVertexShaderConstant ("g_Texture4Resolution", g_Texture4Resolution, 4);
    services->setVertexShaderConstant ("g_Texture5Resolution", g_Texture5Resolution, 4);
    services->setVertexShaderConstant ("g_Texture6Resolution", g_Texture6Resolution, 4);
    services->setVertexShaderConstant ("g_Texture7Resolution", g_Texture7Resolution, 4);

    services->setVertexShaderConstant ("g_Texture0Rotation", g_Texture0Rotation, 4);
    services->setVertexShaderConstant ("g_Texture1Rotation", g_Texture1Rotation, 4);
    services->setVertexShaderConstant ("g_Texture2Rotation", g_Texture2Rotation, 4);
    services->setVertexShaderConstant ("g_Texture3Rotation", g_Texture3Rotation, 4);
    services->setVertexShaderConstant ("g_Texture4Rotation", g_Texture4Rotation, 4);
    services->setVertexShaderConstant ("g_Texture5Rotation", g_Texture5Rotation, 4);
    services->setVertexShaderConstant ("g_Texture6Rotation", g_Texture6Rotation, 4);
    services->setVertexShaderConstant ("g_Texture7Rotation", g_Texture7Rotation, 4);

    services->setPixelShaderConstant ("g_Texture0", &g_Texture0, 1);
    services->setPixelShaderConstant ("g_Texture1", &g_Texture1, 1);
    services->setPixelShaderConstant ("g_Texture2", &g_Texture2, 1);
    services->setPixelShaderConstant ("g_Texture3", &g_Texture3, 1);
    services->setPixelShaderConstant ("g_Texture4", &g_Texture4, 1);
    services->setPixelShaderConstant ("g_Texture5", &g_Texture5, 1);
    services->setPixelShaderConstant ("g_Texture6", &g_Texture6, 1);
    services->setPixelShaderConstant ("g_Texture7", &g_Texture7, 1);
}

const std::string CImage::Type = "image";