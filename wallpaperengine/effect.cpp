#include <wallpaperengine/fs/utils.h>

#include "shaders/compiler.h"
#include "effect.h"
#include "irrlicht.h"

extern irr::f32 g_Time;

namespace wp
{
    effect::effect (json json_data)
    {
        json::const_iterator file = json_data.find ("file");
        json::const_iterator pass = json_data.find ("passes");

        if (file != json_data.end () && (*file).is_string () == true)
        {
            this->m_file = (*file).get <std::string> ().c_str ();
        }

        // passes list is optional
        if (pass == json_data.end () || (*pass).is_array () == false)
            return;

        json::const_iterator curpass = (*pass).begin ();
        json::const_iterator endpass = (*pass).end ();

        if (curpass == endpass)
            return;

        json::const_iterator textures = (*curpass).find ("textures");

        if (textures == (*curpass).end () || (*textures).is_array () == false)
            return;

        json::const_iterator curtex = (*textures).begin ();
        json::const_iterator endtex = (*textures).end ();

        for (; curtex != endtex; curtex ++)
        {
            if ((*curtex).is_null () == true)
            {
                this->m_textures.push_back (nullptr);
            }
            else if ((*curtex).is_string () == true)
            {
                irr::io::path texturepath = ("materials/" + (*curtex).get <std::string> () + ".tex").c_str ();

                this->m_textures.push_back (new wp::texture (texturepath));
            }
        }

        this->m_content = wp::fs::utils::loadFullFile (this->m_file);
        this->m_json = json::parse (this->m_content);

        json::const_iterator passes = this->m_json.find ("passes");

        if (passes == this->m_json.end () || (*passes).is_array () == false)
            return;

        json::const_iterator curpassmaterial = (*passes).begin ();
        json::const_iterator endpassmaterial = (*passes).end ();

        for (; curpassmaterial != endpassmaterial; curpassmaterial ++)
        {
            json::const_iterator material_it = (*curpassmaterial).find ("material");

            if (material_it == (*curpassmaterial).end () || (*material_it).is_string () == false)
                continue;

            irr::io::path material = (*material_it).get <std::string> ().c_str ();
            std::string content = wp::fs::utils::loadFullFile (material);
            json material_json = json::parse (content);

            json::const_iterator shader_passes = material_json.find ("passes");

            if (shader_passes == material_json.end () || (*shader_passes).is_array () == false)
                continue;

            json::const_iterator shaderpasscur = (*shader_passes).begin ();
            json::const_iterator shader = (*shaderpasscur).find ("shader");

            if (shader == (*shaderpasscur).end () || (*shader).is_string () == false)
                continue;

            irr::io::path shaderpath = ("shaders/" + (*shader).get <std::string> ()).c_str ();
            irr::io::path fragpath = shaderpath + ".frag";
            irr::io::path vertpath = shaderpath + ".vert";

            wp::shaders::compiler* fragcompiler = new wp::shaders::compiler (fragpath, wp::shaders::compiler::Type::Type_Pixel, false);
            wp::shaders::compiler* vertcompiler = new wp::shaders::compiler (vertpath, wp::shaders::compiler::Type::Type_Vertex, false);

            wp::irrlicht::driver->getGPUProgrammingServices ()
                ->addHighLevelShaderMaterial (
                    fragcompiler->precompile ().c_str (), "main", irr::video::EVST_VS_2_0,
                    vertcompiler->precompile ().c_str (), "main", irr::video::EPST_PS_2_0,
                    this, irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL, 0, irr::video::EGSL_DEFAULT
                );

            delete fragcompiler;
            delete vertcompiler;
        }
    }

    std::vector<wp::texture*>& effect::getTextureList ()
    {
        return this->m_textures;
    }

    irr::s32 effect::getMaterialType ()
    {
        return this->m_materialType;
    }

    void effect::OnSetConstants (irr::video::IMaterialRendererServices* services, int32_t userData)
    {
        // TODO: IMPLEMENT PROPER SHADER CODE
        irr::f32 g_AnimationSpeed = 0.1f;
        irr::f32 g_Scale = 2.5f;
        irr::f32 g_ScrollSpeed = 0.0f;
        irr::f32 g_Direction = 0.0f;
        irr::f32 g_Strength = 0.07f;
        irr::f32 g_SpecularPower = 1.0f;
        irr::f32 g_SpecularStrength = 1.0f;
        irr::f32 g_SpecularColor [3] = {1.0f, 1.0f, 1.0f};
        irr::f32 g_Texture1Resolution [4] = {1.0f, 1.0f, 1.0f, 1.0f};
        irr::f32 g_Texture0 = 0;
        irr::f32 g_Texture1 = 1;
        irr::f32 g_Texture2 = 2;

        irr::video::IVideoDriver* driver = services->getVideoDriver ();

        irr::core::matrix4 worldViewProj;
        worldViewProj = driver->getTransform(irr::video::ETS_PROJECTION);
        worldViewProj *= driver->getTransform(irr::video::ETS_VIEW);
        worldViewProj *= driver->getTransform(irr::video::ETS_WORLD);

        services->setVertexShaderConstant ("g_AnimationSpeed", &g_AnimationSpeed, 1);
        services->setVertexShaderConstant ("g_Scale", &g_Scale, 1);
        services->setVertexShaderConstant ("g_ScrollSpeed", &g_ScrollSpeed, 1);
        services->setVertexShaderConstant ("g_Direction", &g_Direction, 1);
        services->setVertexShaderConstant ("g_Time", &g_Time, 1);
        services->setVertexShaderConstant ("g_ModelViewProjectionMatrix", worldViewProj.pointer(), 16);
        services->setVertexShaderConstant ("g_Texture0Resolution", g_Texture1Resolution, 4);
        services->setVertexShaderConstant ("g_Texture1Resolution", g_Texture1Resolution, 4);
        services->setVertexShaderConstant ("g_Texture2Resolution", g_Texture1Resolution, 4);

        // TODO: Support up to 7 materials (as wallpaper engine)
        services->setPixelShaderConstant ("g_Strength", &g_Strength, 1);
        services->setPixelShaderConstant ("g_SpecularPower", &g_SpecularPower, 1);
        services->setPixelShaderConstant ("g_SpecularStrength", &g_SpecularStrength, 1);
        services->setPixelShaderConstant ("g_SpecularColor", g_SpecularColor, 3);
        services->setPixelShaderConstant ("g_Texture0", &g_Texture0, 1);
        services->setPixelShaderConstant ("g_Texture1", &g_Texture1, 1);
        services->setPixelShaderConstant ("g_Texture2", &g_Texture2, 1);
    }
};