#include "CImage.h"

#include "WallpaperEngine/Render/Shaders/Compiler.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableFloat.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableInteger.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector2.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector3.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector4.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableFloatPointer.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector2Pointer.h"

#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector3.h"

using namespace WallpaperEngine;

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

using namespace WallpaperEngine::Render::Objects;
using namespace WallpaperEngine::Render::Shaders::Variables;

extern irr::f32 g_Time;

CImage::CImage (CScene* scene, Core::Objects::CImage* image) :
    Render::CObject (scene, Type, image),
    m_image (image),
    m_passes (0)
{
    // TODO: INITIALIZE NEEDED EFFECTS AND PROPERLY CALCULATE THESE?
    irr::f32 xright     = this->m_image->getOrigin ().X;
    irr::f32 xleft      = -this->m_image->getOrigin ().X;
    irr::f32 ztop       = this->m_image->getOrigin ().Y;
    irr::f32 zbottom    = -this->m_image->getOrigin ().Y;
    irr::f32 z          = this->m_image->getOrigin ().Z;

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
            3, 2, 1, 0
    };

    irr::video::IVideoDriver* driver = SceneManager->getVideoDriver ();

    auto cur = this->m_materials.begin ();
    auto end = this->m_materials.end ();

    auto textureCur = this->m_renderTextures.begin ();
    auto textureEnd = this->m_renderTextures.end ();

    for (; cur != end; cur ++)
    {
        if (textureCur == textureEnd)
        {
            driver->setRenderTarget (irr::video::ERT_FRAME_BUFFER, false, false);
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
    auto cur = this->m_image->getMaterial ()->getPasses ().begin ();
    auto end = this->m_image->getMaterial ()->getPasses ().end ();

    for (; cur != end; cur++)
    {
        this->generatePass (*cur);
    }

    auto effectCur = this->m_image->getEffects ().begin ();
    auto effectEnd = this->m_image->getEffects ().end ();

    for (; effectCur != effectEnd; effectCur++)
    {
        auto materialCur = (*effectCur)->getMaterials ().begin ();
        auto materialEnd = (*effectCur)->getMaterials ().end ();

        for (; materialCur != materialEnd; materialCur++)
        {
            cur = (*materialCur)->getPasses ().begin ();
            end = (*materialCur)->getPasses ().end ();

            for (; cur != end; cur++)
            {
                this->generatePass (*cur);
            }
        }
    }
}

void CImage::generatePass (Core::Objects::Images::Materials::CPass* pass)
{
    std::vector<std::string> textures = pass->getTextures ();
    irr::video::SMaterial material;

    auto texturesCur = textures.begin ();
    auto texturesEnd = textures.end ();

    for (int textureNumber = 0; texturesCur != texturesEnd; texturesCur ++, textureNumber ++)
    {
        irr::io::path texturepath = this->getScene ()->getContext ()->resolveMaterials (*texturesCur);
        irr::video::ITexture* texture = nullptr;

        if (textureNumber == 0 && this->m_passes > 0)
        {
            irr::video::ITexture* originalTexture = this->getScene ()->getContext ()->getDevice ()->getVideoDriver ()->getTexture (texturepath);

            texture = this->getScene ()->getContext ()->getDevice ()->getVideoDriver ()->addRenderTargetTexture (
                irr::core::dimension2d<irr::u32> (
                    originalTexture->getSize ().Width,
                    originalTexture->getSize ().Height
                ), ("_RT_" + this->m_image->getName () + std::to_string (textureNumber) + "_" + std::to_string (this->m_passes)).c_str ()
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
    irr::io::path vertpath = this->getScene ()->getContext ()->resolveVertexShader (pass->getShader ());
    irr::io::path fragpath = this->getScene ()->getContext ()->resolveFragmentShader (pass->getShader ());

    auto vertexShader = new Render::Shaders::Compiler (this->getScene ()->getContext (), vertpath, Render::Shaders::Compiler::Type::Type_Vertex, pass->getCombos (), false);
    auto pixelShader = new Render::Shaders::Compiler (this->getScene ()->getContext (), fragpath, Render::Shaders::Compiler::Type::Type_Pixel, pass->getCombos (), false);

    material.MaterialType = (irr::video::E_MATERIAL_TYPE)
        this->getScene ()->getContext ()->getDevice ()->getVideoDriver ()->getGPUProgrammingServices ()->addHighLevelShaderMaterial (
            vertexShader->precompile ().c_str (), "main", irr::video::EVST_VS_2_0,
            pixelShader->precompile ().c_str (), "main", irr::video::EPST_PS_2_0,
            this, irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL, this->m_passes, irr::video::EGSL_DEFAULT
        );

    // find variables in the shaders and set the value with the constants if possible
    auto cur = pass->getConstants ().begin ();
    auto end = pass->getConstants ().end ();

    for (; cur != end; cur ++)
    {
        CShaderVariable* vertexVar = vertexShader->findParameter ((*cur).first);
        CShaderVariable* pixelVar = pixelShader->findParameter ((*cur).first);

        if (pixelVar)
        {
            if (pixelVar->is <CShaderVariableFloat> () && (*cur).second->is <CShaderConstantFloat> ())
            {
                pixelVar->as <CShaderVariableFloat> ()->setValue (*(*cur).second->as <CShaderConstantFloat> ()->getValue ());
            }
            else if (pixelVar->is <CShaderVariableInteger> () && (*cur).second->is <CShaderConstantInteger> ())
            {
                pixelVar->as <CShaderVariableInteger> ()->setValue (*(*cur).second->as <CShaderConstantInteger> ()->getValue ());
            }
            else if (pixelVar->is <CShaderVariableVector3> () && (*cur).second->is <CShaderConstantVector3> ())
            {
                pixelVar->as <CShaderVariableVector3> ()->setValue (*(*cur).second->as <CShaderConstantVector3> ()->getValue ());
            }
        }

        if (vertexVar)
        {
            if (vertexVar->is <CShaderVariableFloat> () && (*cur).second->is <CShaderConstantFloat> ())
            {
                vertexVar->as <CShaderVariableFloat> ()->setValue (*(*cur).second->as <CShaderConstantFloat> ()->getValue ());
            }
            else if (vertexVar->is <CShaderVariableInteger> () && (*cur).second->is <CShaderConstantInteger> ())
            {
                vertexVar->as <CShaderVariableInteger> ()->setValue (*(*cur).second->as <CShaderConstantInteger> ()->getValue ());
            }
            else if (vertexVar->is <CShaderVariableVector3> () && (*cur).second->is <CShaderConstantVector3> ())
            {
                vertexVar->as <CShaderVariableVector3> ()->setValue (*(*cur).second->as <CShaderConstantVector3> ()->getValue ());
            }
        }
    }

    // TODO: TAKE INTO ACCOUNT BLENDING AND CULLING METHODS FROM THE JSON
    material.setFlag (irr::video::EMF_LIGHTING, false);
    material.setFlag (irr::video::EMF_BLEND_OPERATION, true);
    material.Wireframe = false;
    material.Lighting = false;

    this->m_vertexShaders.push_back (vertexShader);
    this->m_pixelShaders.push_back (pixelShader);
    this->m_materials.push_back (material);
    this->m_passes ++;
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

    irr::f32 g_Texture0Rotation [4] = { this->m_image->getAngles ().X, this->m_image->getAngles ().Y, this->m_image->getAngles ().Z, this->m_image->getAngles ().Z };
    irr::f32 g_Texture1Rotation [4] = { this->m_image->getAngles ().X, this->m_image->getAngles ().Y, this->m_image->getAngles ().Z, this->m_image->getAngles ().Z };
    irr::f32 g_Texture2Rotation [4] = { this->m_image->getAngles ().X, this->m_image->getAngles ().Y, this->m_image->getAngles ().Z, this->m_image->getAngles ().Z };
    irr::f32 g_Texture3Rotation [4] = { this->m_image->getAngles ().X, this->m_image->getAngles ().Y, this->m_image->getAngles ().Z, this->m_image->getAngles ().Z };
    irr::f32 g_Texture4Rotation [4] = { this->m_image->getAngles ().X, this->m_image->getAngles ().Y, this->m_image->getAngles ().Z, this->m_image->getAngles ().Z };
    irr::f32 g_Texture5Rotation [4] = { this->m_image->getAngles ().X, this->m_image->getAngles ().Y, this->m_image->getAngles ().Z, this->m_image->getAngles ().Z };
    irr::f32 g_Texture6Rotation [4] = { this->m_image->getAngles ().X, this->m_image->getAngles ().Y, this->m_image->getAngles ().Z, this->m_image->getAngles ().Z };
    irr::f32 g_Texture7Rotation [4] = { this->m_image->getAngles ().X, this->m_image->getAngles ().Y, this->m_image->getAngles ().Z, this->m_image->getAngles ().Z };

    irr::f32 g_Texture0Resolution [4] = { this->m_image->getSize ().X, this->m_image->getSize ().Y, this->m_image->getSize ().X, this->m_image->getSize ().Y };
    irr::f32 g_Texture1Resolution [4] = { this->m_image->getSize ().X, this->m_image->getSize ().Y, this->m_image->getSize ().X, this->m_image->getSize ().Y };
    irr::f32 g_Texture2Resolution [4] = { this->m_image->getSize ().X, this->m_image->getSize ().Y, this->m_image->getSize ().X, this->m_image->getSize ().Y };
    irr::f32 g_Texture3Resolution [4] = { this->m_image->getSize ().X, this->m_image->getSize ().Y, this->m_image->getSize ().X, this->m_image->getSize ().Y };
    irr::f32 g_Texture4Resolution [4] = { this->m_image->getSize ().X, this->m_image->getSize ().Y, this->m_image->getSize ().X, this->m_image->getSize ().Y };
    irr::f32 g_Texture5Resolution [4] = { this->m_image->getSize ().X, this->m_image->getSize ().Y, this->m_image->getSize ().X, this->m_image->getSize ().Y };
    irr::f32 g_Texture6Resolution [4] = { this->m_image->getSize ().X, this->m_image->getSize ().Y, this->m_image->getSize ().X, this->m_image->getSize ().Y };
    irr::f32 g_Texture7Resolution [4] = { this->m_image->getSize ().X, this->m_image->getSize ().Y, this->m_image->getSize ().X, this->m_image->getSize ().Y };

    irr::video::IVideoDriver* driver = services->getVideoDriver ();

    irr::core::matrix4 worldViewProj;
    worldViewProj = driver->getTransform (irr::video::ETS_PROJECTION);
    worldViewProj *= driver->getTransform (irr::video::ETS_VIEW);
    worldViewProj *= driver->getTransform (irr::video::ETS_WORLD);

    Render::Shaders::Compiler* vertexShader = this->m_vertexShaders.at (userData);
    Render::Shaders::Compiler* pixelShader = this->m_pixelShaders.at (userData);

    auto cur = vertexShader->getParameters ().begin ();
    auto end = vertexShader->getParameters ().end ();

    for (; cur != end; cur ++)
    {
        if ((*cur)->is <CShaderVariableInteger> () == true)
        {
            services->setVertexShaderConstant (
                (*cur)->getName ().c_str (),
                (irr::s32*) (*cur)->getValue (),
                (*cur)->getSize ()
            );
        }
        else if (
            (*cur)->is <CShaderVariableFloat> () == true ||
            (*cur)->is <CShaderVariableVector2> () == true ||
            (*cur)->is <CShaderVariableVector3> () == true ||
            (*cur)->is <CShaderVariableVector4> () == true)
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
        if ((*cur)->is <CShaderVariableInteger> () == true)
        {
            services->setPixelShaderConstant (
                (*cur)->getName ().c_str (),
                (irr::s32*) (*cur)->getValue (),
                (*cur)->getSize ()
            );
        }
        else if (
            (*cur)->is <CShaderVariableFloat> () == true ||
            (*cur)->is <CShaderVariableVector2> () == true ||
            (*cur)->is <CShaderVariableVector3> () == true ||
            (*cur)->is <CShaderVariableVector4> () == true)
        {
            services->setPixelShaderConstant (
                (*cur)->getName ().c_str (),
                (irr::f32*) (*cur)->getValue (),
                (*cur)->getSize ()
            );
        }
    }

    cur = this->getScene ()->getContext ()->getShaderVariables ().begin ();
    end = this->getScene ()->getContext ()->getShaderVariables ().end ();

    for (; cur != end; cur ++)
    {
        if ((*cur)->is <CShaderVariableFloatPointer> () == true)
        {
            services->setPixelShaderConstant (
                (*cur)->getName ().c_str (),
                (irr::f32*) (*cur)->getValue (),
                (*cur)->getSize ()
            );
            services->setVertexShaderConstant (
                (*cur)->getName ().c_str (),
                (irr::f32*) (*cur)->getValue (),
                (*cur)->getSize ()
            );
        }
    }

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