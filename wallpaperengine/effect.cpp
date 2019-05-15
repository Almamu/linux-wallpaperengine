#include <wallpaperengine/fs/utils.h>

#include "shaders/compiler.h"
#include "effect.h"
#include "irrlicht.h"
#include "core.h"

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

            this->m_fragShader = new wp::shaders::compiler (fragpath, wp::shaders::compiler::Type::Type_Pixel, false);
            this->m_vertShader = new wp::shaders::compiler (vertpath, wp::shaders::compiler::Type::Type_Vertex, false);

            this->m_materialType = wp::irrlicht::driver->getGPUProgrammingServices ()
                ->addHighLevelShaderMaterial (
                    this->m_vertShader->precompile ().c_str (), "main", irr::video::EVST_VS_2_0,
                    this->m_fragShader->precompile ().c_str (), "main", irr::video::EPST_PS_2_0,
                    this, irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL, 0, irr::video::EGSL_DEFAULT
                );
        }

        json::const_iterator constantvalues = (*curpass).find ("constantshadervalues");

        if (constantvalues != (*curpass).end () && (*constantvalues).is_object () == true)
            this->parseConstantValues ((*constantvalues));

        // last step is creating the actual variables for the shaders
        std::vector <shaders::compiler::ShaderParameter*>::const_iterator cur;
        std::vector <shaders::compiler::ShaderParameter*>::const_iterator end;

        cur = this->m_fragShader->getParameters ().begin ();
        end = this->m_fragShader->getParameters ().end ();

        // first do fragment shaders
        for (; cur != end; cur ++)
        {
            wp::shaders::compiler::ShaderParameter* param = (*cur);
            ShaderParameter* parameter = new ShaderParameter;
            void* defaultValue = param->defaultValue;

            std::map<std::string,void*>::const_iterator constant = this->m_constants.find (param->identifierName);

            if (constant != this->m_constants.end ())
                defaultValue = (*constant).second;

            parameter->value = nullptr;

            if (param->type == "vec4")
            {
                irr::core::vector3df* vec = (irr::core::vector3df*) defaultValue;
                irr::f32* val = new irr::f32 [4];

                val [0] = vec->X;
                val [1] = vec->Y;
                val [2] = vec->Z;
                val [3] = 0.0f;

                parameter->value = val;
                parameter->type = ParameterType::TYPE_VEC4;
            }
            else if (param->type == "vec3")
            {
                irr::core::vector3df* vec = (irr::core::vector3df*) defaultValue;
                irr::f32* val = new irr::f32 [3];

                val [0] = vec->X;
                val [1] = vec->Y;
                val [2] = vec->Z;

                parameter->value = val;
                parameter->type = ParameterType::TYPE_VEC3;
            }
            else if (param->type == "vec2")
            {
                irr::core::vector2df* vec = (irr::core::vector2df*) defaultValue;
                irr::f32* val = new irr::f32 [2];

                val [0] = vec->X;
                val [1] = vec->Y;

                parameter->value = val;
                parameter->type = ParameterType::TYPE_VEC2;
            }
            else if (param->type == "float")
            {
                irr::f32* org = (irr::f32*) defaultValue;
                irr::f32* val = new irr::f32;

                *val = *org;

                parameter->value = val;
                parameter->type = ParameterType::TYPE_FLOAT;
            }
            else if (param->type == "int")
            {
                irr::s32* org = (irr::s32*) defaultValue;
                irr::s32* val = new irr::s32;

                *val = *org;

                parameter->value = val;
                parameter->type = ParameterType::TYPE_INT;
            }
            else if (param->type == "sampler2D")
            {
                // sampler2D are textures from wallpaper engine
                // so these can be ignored as they are later on defined
                continue;
            }

            this->m_pixelVariables.insert (std::pair <std::string, ShaderParameter*> (param->variableName, parameter));
        }

        cur = this->m_vertShader->getParameters ().begin ();
        end = this->m_vertShader->getParameters ().end ();

        // second do vertex shaders
        for (;cur != end; cur ++)
        {
            wp::shaders::compiler::ShaderParameter* param = (*cur);

            if (param == nullptr)
                continue;

            void* defaultValue = param->defaultValue;

            std::map<std::string,void*>::const_iterator constant = this->m_constants.find (param->identifierName);

            if (constant != this->m_constants.end ())
                defaultValue = (*constant).second;

            ShaderParameter* parameter = new ShaderParameter;

            parameter->value = nullptr;

            if (param->type == "vec4")
            {
                irr::core::vector3df* vec = (irr::core::vector3df*) defaultValue;

                parameter->value = new irr::f32 [4];

                (*(irr::f32**) parameter->value) [0] = vec->X;
                (*(irr::f32**) parameter->value) [1] = vec->Y;
                (*(irr::f32**) parameter->value) [2] = vec->Z;
                (*(irr::f32**) parameter->value) [3] = 0.0f;

                parameter->type = ParameterType::TYPE_VEC4;
            }
            else if (param->type == "vec3")
            {
                irr::core::vector3df* vec = (irr::core::vector3df*) defaultValue;

                parameter->value = new irr::f32 [3];

                (*(irr::f32**) parameter->value) [0] = vec->X;
                (*(irr::f32**) parameter->value) [1] = vec->Y;
                (*(irr::f32**) parameter->value) [2] = vec->Z;

                parameter->type = ParameterType::TYPE_VEC3;
            }
            else if (param->type == "vec2")
            {
                irr::core::vector2df* vec = (irr::core::vector2df*) defaultValue;

                parameter->value = new irr::f32 [2];

                (*(irr::f32**) parameter->value) [0] = vec->X;
                (*(irr::f32**) parameter->value) [1] = vec->Y;

                parameter->type = ParameterType::TYPE_VEC2;
            }
            else if (param->type == "float")
            {
                parameter->value = new irr::f32;
                *(irr::f32*) parameter->value = *(irr::f32*) defaultValue;

                parameter->type = ParameterType::TYPE_FLOAT;
            }
            else if (param->type == "int")
            {
                parameter->value = new irr::s32;
                *(irr::s32*) parameter->value = *(irr::s32*) defaultValue;

                parameter->type = ParameterType::TYPE_INT;
            }
            else if (param->type == "sampler2D")
            {
                // sampler2D are textures from wallpaper engine
                // so these can be ignored as they are later on defined
                continue;
            }

            this->m_vertexVariables.insert (std::pair <std::string, ShaderParameter*> (param->variableName, parameter));
        }
    }

    void effect::parseConstantValues (json data)
    {
/*
								"ui_editor_properties_animation_speed" : 0.10000000149011612,
								"ui_editor_properties_ripple_scale" : 2.5,
								"ui_editor_properties_ripple_strength" : 0.070000000298023224
*/
        json::const_iterator cur = data.begin ();
        json::const_iterator end = data.end ();

        for (; cur != end; cur ++)
        {
            std::string name = cur.key ();
            void* value = nullptr;

            if ((*cur).is_number_float () == true)
            {
                value = new irr::f32;
                *(irr::f32*) value = (*cur).get <irr::f32> ();
            }
            else if ((*cur).is_number_integer () == true)
            {
                value = new irr::s32;
                *(irr::s32*) value = (*cur).get <irr::s32> ();
            }
            else if ((*cur).is_string () == true)
            {
                value = new irr::core::vector3df;
                *(irr::core::vector3df*) value = core::ato3vf ((*cur).get <std::string> ().c_str ());
            }

            this->m_constants.insert (std::pair <std::string, void*> (name, value));
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

        /*
            THESE ARE THE VARIABLES AVAILABLE ON SHADERS

            uniform float g_Alpha;

            uniform vec3 g_Color;

            // Application time, starts with 0
            uniform float g_Time;

            // Maps 24 hrs to [0, 1]
            uniform float g_Daytime;

            uniform vec2 g_TexelSize;
            uniform vec2 g_TexelSizeHalf;

            uniform mat4 g_ModelMatrix;
            uniform mat4 g_ViewProjectionMatrix;
            uniform mat4 g_ModelViewProjectionMatrix;
            uniform mat4 g_ModelViewProjectionMatrixInverse;

            uniform vec3 g_EyePosition;
            uniform vec3 g_ViewUp;
            uniform vec3 g_ViewRight;

            // Samplers that map to the textures[] array in the material
            uniform sampler2D g_Texture0;
            uniform sampler2D g_Texture1;
            uniform sampler2D g_Texture2;
            uniform sampler2D g_Texture3;
            uniform sampler2D g_Texture4;
            uniform sampler2D g_Texture5;
            uniform sampler2D g_Texture6;
            uniform sampler2D g_Texture7;

            // For sprite sheets (GIFs)
            uniform vec4 g_Texture0Rotation;
            uniform vec4 g_Texture1Rotation;
            uniform vec4 g_Texture2Rotation;
            uniform vec4 g_Texture3Rotation;
            uniform vec4 g_Texture4Rotation;
            uniform vec4 g_Texture5Rotation;
            uniform vec4 g_Texture6Rotation;
            uniform vec4 g_Texture7Rotation;

            uniform vec2 g_Texture0Translation;
            uniform vec2 g_Texture1Translation;
            uniform vec2 g_Texture2Translation;
            uniform vec2 g_Texture3Translation;
            uniform vec2 g_Texture4Translation;
            uniform vec2 g_Texture5Translation;
            uniform vec2 g_Texture6Translation;
            uniform vec2 g_Texture7Translation;

            uniform vec4 g_LightsColorRadius[4]; // color in XYZ, radius in W
            uniform vec3 g_LightsPosition[4];

            uniform vec3 g_LightAmbientColor;
            uniform vec3 g_LightSkylightColor;

            // lower frequencies at lower indices
            uniform float g_AudioSpectrum16Left[16];
            uniform float g_AudioSpectrum16Right[16];
            uniform float g_AudioSpectrum32Left[32];
            uniform float g_AudioSpectrum32Right[32];
            uniform float g_AudioSpectrum64Left[64];
            uniform float g_AudioSpectrum64Right[64];

            // Normalized in UV space [0, 1]
            uniform vec2 g_PointerPosition;

        */

        services->setVertexShaderConstant ("g_Time", &g_Time, 1);
        services->setPixelShaderConstant  ("g_Time", &g_Time, 1);

        std::map<std::string, ShaderParameter*>::const_iterator vertcur = this->m_vertexVariables.begin ();
        std::map<std::string, ShaderParameter*>::const_iterator vertend = this->m_vertexVariables.end ();

        for (; vertcur != vertend; vertcur ++)
        {
            switch ((*vertcur).second->type)
            {
                case ParameterType::TYPE_FLOAT:
                    services->setVertexShaderConstant ((*vertcur).first.c_str (), (irr::f32*) (*vertcur).second->value, 1);
                    break;
                case ParameterType::TYPE_VEC2:
                    services->setVertexShaderConstant ((*vertcur).first.c_str (), (irr::f32*) (*vertcur).second->value, 2);
                    break;
                case ParameterType::TYPE_VEC3:
                    services->setVertexShaderConstant ((*vertcur).first.c_str (), (irr::f32*) (*vertcur).second->value, 3);
                    break;
                case ParameterType::TYPE_VEC4:
                    services->setVertexShaderConstant ((*vertcur).first.c_str (), (irr::f32*) (*vertcur).second->value, 4);
                    break;
                case ParameterType::TYPE_INT:
                    services->setVertexShaderConstant ((*vertcur).first.c_str (), (irr::s32*) (*vertcur).second->value, 1);
                    break;

            }
        }

        std::map<std::string, ShaderParameter*>::const_iterator pixelcur = this->m_pixelVariables.begin ();
        std::map<std::string, ShaderParameter*>::const_iterator pixelend = this->m_pixelVariables.end ();

        for (; pixelcur != pixelend; pixelcur ++)
        {
            switch ((*pixelcur).second->type)
            {
                case ParameterType::TYPE_FLOAT:
                    services->setPixelShaderConstant ((*pixelcur).first.c_str (), (irr::f32*) (*pixelcur).second->value, 1);
                    break;
                case ParameterType::TYPE_VEC2:
                    services->setPixelShaderConstant ((*pixelcur).first.c_str (), (irr::f32*) (*pixelcur).second->value, 2);
                    break;
                case ParameterType::TYPE_VEC3:
                    services->setPixelShaderConstant ((*pixelcur).first.c_str (), (irr::f32*) (*pixelcur).second->value, 3);
                    break;
                case ParameterType::TYPE_VEC4:
                    services->setPixelShaderConstant ((*pixelcur).first.c_str (), (irr::f32*) (*pixelcur).second->value, 4);
                    break;
                case ParameterType::TYPE_INT:
                    services->setPixelShaderConstant ((*pixelcur).first.c_str (), (irr::s32*) (*pixelcur).second->value, 1);
                    break;

            }
        }

        /*services->setVertexShaderConstant ("g_AnimationSpeed", &g_AnimationSpeed, 1);
        services->setVertexShaderConstant ("g_Scale", &g_Scale, 1);
        services->setVertexShaderConstant ("g_ScrollSpeed", &g_ScrollSpeed, 1);
        services->setVertexShaderConstant ("g_Direction", &g_Direction, 1);*/
        services->setVertexShaderConstant ("g_ModelViewProjectionMatrix", worldViewProj.pointer(), 16);


        services->setVertexShaderConstant ("g_Texture0Resolution", g_Texture1Resolution, 4);
        services->setVertexShaderConstant ("g_Texture1Resolution", g_Texture1Resolution, 4);
        services->setVertexShaderConstant ("g_Texture2Resolution", g_Texture1Resolution, 4);

        // TODO: Support up to 7 materials (as wallpaper engine)
        /*services->setPixelShaderConstant ("g_Strength", &g_Strength, 1);
        services->setPixelShaderConstant ("g_SpecularPower", &g_SpecularPower, 1);
        services->setPixelShaderConstant ("g_SpecularStrength", &g_SpecularStrength, 1);
        services->setPixelShaderConstant ("g_SpecularColor", g_SpecularColor, 3);*/
        services->setPixelShaderConstant ("g_Texture0", &g_Texture0, 1);
        services->setPixelShaderConstant ("g_Texture1", &g_Texture1, 1);
        services->setPixelShaderConstant ("g_Texture2", &g_Texture2, 1);
    }
};