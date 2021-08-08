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

extern irr::f32 g_Time;

CPass::CPass (Irrlicht::CContext* context, CMaterial* material, Core::Objects::Images::Materials::CPass* pass, irr::video::ITexture* texture) :
    m_material (material),
    m_pass (pass),
    m_context (context),
    m_inputTexture (texture),
    m_outputTexture (nullptr)
{
    this->m_outputTexture = this->m_context->getDevice ()->getVideoDriver ()->addRenderTargetTexture (
        this->m_inputTexture->getSize (),
        (
            "_rt_WALLENGINELINUX_OUTPUT_" +
            std::to_string (this->m_material->getImage ()->getImage ()->getId ()) + "_" +
            std::to_string (this->m_material->getImage ()->getEffects ().size ()) +
            "_pass_output"
        ).c_str ()
    );

    irr::io::path vertPath = this->m_context->resolveVertexShader (pass->getShader ());
    irr::io::path fragPath = this->m_context->resolveFragmentShader (pass->getShader ());
    // register fragment shader
    this->m_fragShader = new Render::Shaders::Compiler (
        this->m_context, fragPath, Render::Shaders::Compiler::Type::Type_Pixel,
        pass->getCombos (), pass->getConstants (), false
    );
    // compile fragment shader
    this->m_fragShader->precompile ();
    // register vertex shader, combos come from the fragment as it can sometimes define them
    this->m_vertShader = new Render::Shaders::Compiler (
            this->m_context, vertPath, Render::Shaders::Compiler::Type::Type_Vertex,
            this->m_fragShader->getCombos (), pass->getConstants (), false
    );
    // compile vertex shader
    this->m_vertShader->precompile ();
    this->setupTextures ();
    // initialize material data and compile shader used for this pass
    this->m_irrlichtMaterial.Wireframe = false;
    this->m_irrlichtMaterial.Lighting = false;
    this->m_irrlichtMaterial.setFlag (irr::video::EMF_LIGHTING, false);
    this->m_irrlichtMaterial.setFlag (irr::video::EMF_BLEND_OPERATION, true);
    this->m_irrlichtMaterial.MaterialType = (irr::video::E_MATERIAL_TYPE)
        this->m_context->getDevice ()->getVideoDriver ()->getGPUProgrammingServices ()->addHighLevelShaderMaterial (
            this->m_vertShader->getCompiled ().c_str (), "main", irr::video::EVST_VS_2_0,
            this->m_fragShader->getCompiled ().c_str (), "main", irr::video::EPST_PS_2_0,
            this, irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL, 0, irr::video::EGSL_DEFAULT
        );

    this->setupShaderVariables ();
}

irr::video::ITexture *CPass::getOutputTexture () const
{
    return this->m_outputTexture;
}

irr::video::ITexture* CPass::getInputTexture () const
{
    return this->m_inputTexture;
}

const irr::video::SMaterial& CPass::getMaterial () const
{
    return this->m_irrlichtMaterial;
}

void CPass::OnSetConstants (irr::video::IMaterialRendererServices *services, int32_t userData)
{
    const Core::Objects::CImage* image = this->m_material->getImage ()->getImage ();

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

    auto textureCur = this->m_textures.begin ();
    auto textureEnd = this->m_textures.end ();

    char resolution [22];
    char sampler [12];
    char rotation [20];
    char translation [23];

    irr::f32 textureRotation [4] = { image->getAngles ().X, image->getAngles ().Y, image->getAngles ().Z, image->getAngles ().Z };

    for (int index = 0; textureCur != textureEnd; textureCur ++, index ++)
    {
        // TODO: CHECK THESE VALUES, DOCUMENTATION SAYS THAT FIRST TWO ELEMENTS SHOULD BE WIDTH AND HEIGHT
        // TODO: BUT IN REALITY THEY DO NOT SEEM TO BE THAT, NOT HAVING SUPPORT FOR ATTRIBUTES DOESN'T HELP EITHER
        irr::f32 textureResolution [4] = {
            1.0, -1.0, 1.0, 1.0
        };
        irr::f32 textureTranslation [2] = {
            0, 0
        };

        sprintf (resolution, "g_Texture%dResolution", index);
        sprintf (sampler, "g_Texture%d", index);
        sprintf (rotation, "g_Texture%dRotation", index);
        sprintf (translation, "g_Texture%dTranslation", index);

        services->setVertexShaderConstant (resolution, textureResolution, 4);
        services->setPixelShaderConstant (resolution, textureResolution, 4);
        services->setVertexShaderConstant (sampler, &index, 1);
        services->setPixelShaderConstant (sampler, &index, 1);
        services->setVertexShaderConstant (rotation, textureRotation, 4);
        services->setPixelShaderConstant (rotation, textureRotation, 4);
        services->setVertexShaderConstant (translation, textureTranslation, 2);
        services->setPixelShaderConstant (translation, textureTranslation, 2);
    }

    // set variables for time
    services->setVertexShaderConstant ("g_Time", &g_Time, 1);
    services->setPixelShaderConstant ("g_Time", &g_Time, 1);
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

void CPass::setupTextures ()
{
    auto textureCur = this->m_pass->getTextures ().begin ();
    auto textureEnd = this->m_pass->getTextures ().end ();

    for (int textureNumber = 0; textureCur != textureEnd; textureCur ++, textureNumber ++)
    {
        irr::video::ITexture* texture = this->m_inputTexture;

        // XXXHACK: TODO: PROPERLY DETECT WHERE THE INPUT TEXTURE SHOULD BE USED IN THE SHADER GRAPH
        if (textureNumber != 0)
        {
            irr::io::path texturepath = this->m_context->resolveMaterials (*textureCur);
            texture = this->m_context->getDevice ()->getVideoDriver ()->getTexture (texturepath);
        }

        this->m_irrlichtMaterial.setTexture (textureNumber, texture);
        this->m_textures.push_back (texture);
    }
}