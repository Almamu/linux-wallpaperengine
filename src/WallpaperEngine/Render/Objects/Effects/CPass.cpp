#include "CPass.h"

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

using namespace WallpaperEngine::Core::Objects::Effects::Constants;
using namespace WallpaperEngine::Render::Shaders::Variables;

using namespace WallpaperEngine::Render::Objects::Effects;

CPass::CPass (Irrlicht::CContext* context, CMaterial* material, Core::Objects::Images::Materials::CPass* pass) :
    m_material (material),
    m_pass (pass),
    m_context (context),
    m_inputTexture (nullptr),
    m_outputTexture (nullptr)
{
    irr::io::path vertPath = this->m_context->resolveVertexShader (pass->getShader ());
    irr::io::path fragPath = this->m_context->resolveFragmentShader (pass->getShader ());
    // register fragment shader
    this->m_fragShader = new Render::Shaders::Compiler (
        this->m_context, vertPath, Render::Shaders::Compiler::Type::Type_Pixel, pass->getCombos (), false
    );
    // register vertex shader
    this->m_vertShader = new Render::Shaders::Compiler (
        this->m_context, fragPath, Render::Shaders::Compiler::Type::Type_Vertex, pass->getCombos (), false
    );

    // initialize material data and compile shader used for this pass
    this->m_irrlichtMaterial.Wireframe = false;
    this->m_irrlichtMaterial.Lighting = false;
    this->m_irrlichtMaterial.setFlag (irr::video::EMF_LIGHTING, false);
    this->m_irrlichtMaterial.setFlag (irr::video::EMF_BLEND_OPERATION, true);
    this->m_irrlichtMaterial.MaterialType = (irr::video::E_MATERIAL_TYPE)
        this->m_context->getDevice ()->getVideoDriver ()->getGPUProgrammingServices ()->addHighLevelShaderMaterial (
            this->m_vertShader->precompile ().c_str (), "main", irr::video::EVST_VS_2_0,
            this->m_fragShader->precompile ().c_str (), "main", irr::video::EPST_PS_2_0,
            this, irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL, 0, irr::video::EGSL_DEFAULT
        );

    this->setupShaderVariables ();
}

const irr::video::ITexture* CPass::getOutputTexture () const
{
    return this->m_outputTexture;
}

const irr::video::ITexture* CPass::getInputTexture () const
{
    return this->m_inputTexture;
}

const irr::video::SMaterial& CPass::getMaterial () const
{
    return this->m_irrlichtMaterial;
}

void CPass::OnSetConstants (irr::video::IMaterialRendererServices *services, int32_t userData)
{
    irr::f32 g_Texture0 = 0;
    irr::f32 g_Texture1 = 1;
    irr::f32 g_Texture2 = 2;
    irr::f32 g_Texture3 = 3;
    irr::f32 g_Texture4 = 4;
    irr::f32 g_Texture5 = 5;
    irr::f32 g_Texture6 = 6;
    irr::f32 g_Texture7 = 7;

    const Core::Objects::CImage* image = this->m_material->getImage ()->getImage ();

    irr::f32 g_Texture0Rotation [4] = { image->getAngles ().X, image->getAngles ().Y, image->getAngles ().Z, image->getAngles ().Z };
    irr::f32 g_Texture1Rotation [4] = { image->getAngles ().X, image->getAngles ().Y, image->getAngles ().Z, image->getAngles ().Z };
    irr::f32 g_Texture2Rotation [4] = { image->getAngles ().X, image->getAngles ().Y, image->getAngles ().Z, image->getAngles ().Z };
    irr::f32 g_Texture3Rotation [4] = { image->getAngles ().X, image->getAngles ().Y, image->getAngles ().Z, image->getAngles ().Z };
    irr::f32 g_Texture4Rotation [4] = { image->getAngles ().X, image->getAngles ().Y, image->getAngles ().Z, image->getAngles ().Z };
    irr::f32 g_Texture5Rotation [4] = { image->getAngles ().X, image->getAngles ().Y, image->getAngles ().Z, image->getAngles ().Z };
    irr::f32 g_Texture6Rotation [4] = { image->getAngles ().X, image->getAngles ().Y, image->getAngles ().Z, image->getAngles ().Z };
    irr::f32 g_Texture7Rotation [4] = { image->getAngles ().X, image->getAngles ().Y, image->getAngles ().Z, image->getAngles ().Z };

    irr::f32 g_Texture0Resolution [4] = { image->getSize ().X, image->getSize ().Y, image->getSize ().X, image->getSize ().Y };
    irr::f32 g_Texture1Resolution [4] = { image->getSize ().X, image->getSize ().Y, image->getSize ().X, image->getSize ().Y };
    irr::f32 g_Texture2Resolution [4] = { image->getSize ().X, image->getSize ().Y, image->getSize ().X, image->getSize ().Y };
    irr::f32 g_Texture3Resolution [4] = { image->getSize ().X, image->getSize ().Y, image->getSize ().X, image->getSize ().Y };
    irr::f32 g_Texture4Resolution [4] = { image->getSize ().X, image->getSize ().Y, image->getSize ().X, image->getSize ().Y };
    irr::f32 g_Texture5Resolution [4] = { image->getSize ().X, image->getSize ().Y, image->getSize ().X, image->getSize ().Y };
    irr::f32 g_Texture6Resolution [4] = { image->getSize ().X, image->getSize ().Y, image->getSize ().X, image->getSize ().Y };
    irr::f32 g_Texture7Resolution [4] = { image->getSize ().X, image->getSize ().Y, image->getSize ().X, image->getSize ().Y };

    irr::video::IVideoDriver* driver = services->getVideoDriver ();

    irr::core::matrix4 worldViewProj;
    worldViewProj = driver->getTransform (irr::video::ETS_PROJECTION);
    worldViewProj *= driver->getTransform (irr::video::ETS_VIEW);
    worldViewProj *= driver->getTransform (irr::video::ETS_WORLD);

    auto cur = this->m_vertShader->getParameters ().begin ();
    auto end = this->m_vertShader->getParameters ().end ();

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

    cur = this->m_fragShader->getParameters ().begin ();
    end = this->m_fragShader->getParameters ().end ();

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

    cur = this->m_context->getShaderVariables ().begin ();
    end = this->m_context->getShaderVariables ().end ();

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

void CPass::setupShaderVariables ()
{
    // find variables in the shaders and set the value with the constants if possible
    auto cur = this->m_pass->getConstants ().begin ();
    auto end = this->m_pass->getConstants ().end ();

    for (; cur != end; cur ++)
    {
        CShaderVariable* vertexVar = this->m_vertShader->findParameter ((*cur).first);
        CShaderVariable* pixelVar = this->m_fragShader->findParameter ((*cur).first);

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
}