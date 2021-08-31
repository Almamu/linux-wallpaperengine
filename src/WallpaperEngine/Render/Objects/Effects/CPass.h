#pragma once

#include <glm/gtc/type_ptr.hpp>
#include <utility>

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
        enum UniformType
        {
            Float = 0,
            Matrix4 = 1,
            Integer = 2,
            Vector2 = 3,
            Vector3 = 4,
            Vector4 = 5,
            Double = 6
        };

        class UniformEntry
        {
        public:
            UniformEntry (GLint id, std::string name, UniformType type, const void* value) :
                id (id), name (std::move (name)), type (type), value (value) { }

            GLint id;
            std::string name;
            UniformType type;
            const void* value;
        };

        static GLuint compileShader (Render::Shaders::Compiler* shader, GLuint type);
        void setupTextures ();
        void setupShaders ();
        void setupShaderVariables ();
        void setupUniforms ();
        void addUniform (const std::string& name, int value);
        void addUniform (const std::string& name, double value);
        void addUniform (const std::string& name, float value);
        void addUniform (const std::string& name, glm::vec2 value);
        void addUniform (const std::string& name, glm::vec3 value);
        void addUniform (const std::string& name, glm::vec4 value);
        void addUniform (const std::string& name, glm::mat4 value);
        void addUniform (const std::string& name, const int* value);
        void addUniform (const std::string& name, const double* value);
        void addUniform (const std::string& name, const float* value);
        void addUniform (const std::string& name, const glm::vec2* value);
        void addUniform (const std::string& name, const glm::vec3* value);
        void addUniform (const std::string& name, const glm::vec4* value);
        void addUniform (const std::string& name, const glm::mat4* value);
        template <typename T> void addUniform (const std::string& name, UniformType type, T value);
        template <typename T> void addUniform (const std::string& name, UniformType type, T* value);

        CMaterial* m_material;
        Core::Objects::Images::Materials::CPass* m_pass;
        std::vector<CTexture*> m_textures;
        std::map<GLint,CShaderVariable*> m_variables;
        std::map<GLint,CShaderVariable*> m_attribs;
        std::vector<UniformEntry*> m_uniforms;
        glm::mat4 m_modelViewProjectionMatrix;

        Render::Shaders::Compiler* m_fragShader;
        Render::Shaders::Compiler* m_vertShader;

        GLuint m_programID;

        // shader variables used temporary
        GLint g_Texture0Rotation;
        GLint g_Texture0Translation;
        GLint a_TexCoord;
        GLint a_Position;
        GLint positionAttribute;
    };
}
