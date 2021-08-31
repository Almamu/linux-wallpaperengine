#pragma once

#include <glm/gtc/type_ptr.hpp>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"
#include "WallpaperEngine/Render/Objects/Effects/CMaterial.h"
#include "WallpaperEngine/Render/Shaders/Compiler.h"
#include "WallpaperEngine/Assets/CTexture.h"

namespace WallpaperEngine::Render::Objects::Effects
{
    using namespace WallpaperEngine::Assets;
    using namespace WallpaperEngine::Render::Shaders::Variables;

    class CMaterial;

    class CPass
    {
    public:
        CPass (CMaterial* material, Core::Objects::Images::Materials::CPass* pass);

        void render (GLuint drawTo, GLuint input);

    private:
        static GLuint compileShader (Render::Shaders::Compiler* shader, GLuint type);
        void setupTextures ();
        void setupShaders ();
        void setupShaderVariables ();

        CMaterial* m_material;
        Core::Objects::Images::Materials::CPass* m_pass;
        std::vector<CTexture*> m_textures;
        std::map<GLint,CShaderVariable*> m_variables;
        std::map<GLint,CShaderVariable*> m_attribs;
        std::map<GLint,int> m_uniforms;

        Render::Shaders::Compiler* m_fragShader;
        Render::Shaders::Compiler* m_vertShader;

        GLuint m_programID;

        // shader variables used temporary
        GLint g_Texture0;
        GLint g_Texture1;
        GLint g_Texture2;
        GLint g_Time;
        GLint g_Texture0Rotation;
        GLint g_Texture0Translation;
        GLint g_ModelViewProjectionMatrix;
        GLint a_TexCoord;
        GLint a_Position;
        GLint g_UserAlpha;
        GLint g_Brightness;
        GLint positionAttribute;
    };
}
