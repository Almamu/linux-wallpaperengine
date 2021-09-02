#include <sstream>
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

extern float g_Time;

CPass::CPass (CMaterial* material, Core::Objects::Images::Materials::CPass* pass) :
    m_material (material),
    m_pass (pass)
{
    this->m_fragShader = new Render::Shaders::Compiler (
        this->m_material->getImage ()->getContainer (),
        pass->getShader (),
        Shaders::Compiler::Type_Pixel,
        pass->getCombos (),
        pass->getConstants ()
    );
    this->m_fragShader->precompile ();
    this->m_vertShader = new Render::Shaders::Compiler (
        this->m_material->getImage ()->getContainer (),
        pass->getShader (),
        Shaders::Compiler::Type_Vertex,
        this->m_fragShader->getCombos (),
        pass->getConstants ()
    );
    this->m_vertShader->precompile ();

    this->setupTextures ();
    this->setupShaders ();
    this->setupShaderVariables ();
}

void CPass::render (GLuint drawTo, GLuint input)
{
    // clear whatever buffer we're drawing to if we're not drawing to screen
    if (drawTo > 0)
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // set texture blending
    if (this->m_pass->getBlendingMode () == "translucent")
    {
        glEnable (GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else if (this->m_pass->getBlendingMode () == "additive")
    {
        glEnable (GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
    }
    else if (this->m_pass->getBlendingMode () == "normal")
    {
        glEnable (GL_BLEND);
        glBlendFuncSeparate(GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    }
    else
    {
        glDisable (GL_BLEND);
    }

    // set depth testing
    if (this->m_pass->getDepthTest () == "disabled")
    {
        glDisable (GL_DEPTH_TEST);
    }
    else
    {
        glEnable (GL_DEPTH_TEST);
    }

    // update variables used in the render process (like g_ModelViewProjectionMatrix)
    this->m_modelViewProjectionMatrix =
            this->m_material->getImage ()->getScene ()->getCamera ()->getProjection () *
            this->m_material->getImage ()->getScene ()->getCamera ()->getLookAt () *
            glm::mat4 (1.0f);

    // update a_TexCoord and a_Position based on what to draw to
    // this should not be required once we do some prediction on rendering things
    // but for now should be enough
    this->a_TexCoord = (input == this->m_material->getImage ()->getTexture ()->getTextureID ()) ? *this->m_material->getImage ()->getTexCoordBuffer () : *this->m_material->getImage ()->getPassTexCoordBuffer ();
    this->a_Position = (drawTo > 0) ? *this->m_material->getImage ()->getPassVertexBuffer () : *this->m_material->getImage ()->getVertexBuffer ();
    // use the shader we have registered
    glUseProgram (this->m_programID);

    // bind the input texture
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, input);

    // first bind the textures to their sampler place
    {
        // set texture slots for the shader
        auto cur = this->m_textures.begin ();
        auto end = this->m_textures.end ();

        for (int index = 1; cur != end; cur ++, index ++)
        {
            // set the active texture index
            glActiveTexture (GL_TEXTURE0 + index);
            // bind the correct texture there
            glBindTexture (GL_TEXTURE_2D, (*cur)->getTextureID ());
        }
    }

    // add uniforms
    {
        auto cur = this->m_uniforms.begin ();
        auto end = this->m_uniforms.end ();

        for (; cur != end; cur ++)
        {
            switch ((*cur)->type)
            {
                case Double:
                    glUniform1d ((*cur)->id, *static_cast <const double*> ((*cur)->value));
                    break;
                case Float:
                    glUniform1f ((*cur)->id, *static_cast <const float*> ((*cur)->value));
                    break;
                case Integer:
                    glUniform1i ((*cur)->id, *static_cast <const int*> ((*cur)->value));
                    break;
                case Vector4:
                    glUniform4fv ((*cur)->id, 1, glm::value_ptr (*static_cast <const glm::vec4*> ((*cur)->value)));
                    break;
                case Vector3:
                    glUniform3fv ((*cur)->id, 1, glm::value_ptr (*static_cast <const glm::vec3*> ((*cur)->value)));
                    break;
                case Vector2:
                    glUniform2fv ((*cur)->id, 1, glm::value_ptr (*static_cast <const glm::vec2*> ((*cur)->value)));
                    break;
                case Matrix4:
                    glUniformMatrix4fv ((*cur)->id, 1, GL_FALSE, glm::value_ptr (*static_cast <const glm::mat4*> ((*cur)->value)));
                    break;
            }
        }
    }

    if (this->g_Texture0Rotation != -1)
    {
        glUniform2f (this->g_Texture0Rotation, 0.0f, 0.0f);
    }
    if (this->g_Texture0Translation != -1)
    {
        glUniform4f (this->g_Texture0Translation, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    {
        auto cur = this->m_attribs.begin ();
        auto end = this->m_attribs.end ();

        for (; cur != end; cur ++)
        {
            glEnableVertexAttribArray ((*cur)->id);
            glBindBuffer (GL_ARRAY_BUFFER, *(*cur)->value);
            glVertexAttribPointer ((*cur)->id, (*cur)->elements, (*cur)->type, GL_FALSE, 0, nullptr);
        }
    }

    // start actual rendering now
    glBindBuffer (GL_ARRAY_BUFFER, (drawTo > 0) ? *this->m_material->getImage ()->getPassVertexBuffer () : *this->m_material->getImage ()->getVertexBuffer ());
    glDrawArrays (GL_TRIANGLES, 0, 6);

    // disable vertex attribs array
    {
        auto cur = this->m_attribs.begin ();
        auto end = this->m_attribs.end ();

        for (; cur != end; cur ++)
            glDisableVertexAttribArray ((*cur)->id);
    }
}

GLuint CPass::compileShader (Render::Shaders::Compiler* shader, GLuint type)
{
    // reserve shaders in OpenGL
    GLuint shaderID = glCreateShader (type);

    // give shader's source code to OpenGL to be compiled
    const char* sourcePointer = shader->getCompiled ().c_str ();

    glShaderSource (shaderID, 1, &sourcePointer, nullptr);
    glCompileShader (shaderID);

    GLint result = GL_FALSE;
    int infoLogLength = 0;

    // ensure the vertex shader was correctly compiled
    glGetShaderiv (shaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv (shaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0)
    {
        char* logBuffer = new char [infoLogLength + 1];
        // ensure logBuffer ends with a \0
        memset (logBuffer, 0, infoLogLength + 1);
        // get information about the error
        glGetShaderInfoLog (shaderID, infoLogLength, nullptr, logBuffer);
        // throw an exception about the issue
        std::string message = logBuffer;
        // free the buffer
        delete[] logBuffer;
        // throw an exception
        throw std::runtime_error (message);
    }

    return shaderID;
}

void CPass::setupShaders ()
{
    // compile the shaders
    GLuint vertexShaderID = compileShader (this->m_vertShader, GL_VERTEX_SHADER);
    GLuint fragmentShaderID = compileShader (this->m_fragShader, GL_FRAGMENT_SHADER);
    // create the final program
    this->m_programID = glCreateProgram ();
    // link the shaders together
    glAttachShader (this->m_programID, vertexShaderID);
    glAttachShader (this->m_programID, fragmentShaderID);
    glLinkProgram (this->m_programID);
    // check that the shader was properly linked
    GLint result = GL_FALSE;
    int infoLogLength = 0;

    glGetProgramiv (this->m_programID, GL_LINK_STATUS, &result);
    glGetProgramiv (this->m_programID, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0)
    {
        char* logBuffer = new char [infoLogLength + 1];
        // ensure logBuffer ends with a \0
        memset (logBuffer, 0, infoLogLength + 1);
        // get information about the error
        glGetProgramInfoLog (this->m_programID, infoLogLength, nullptr, logBuffer);
        // throw an exception about the issue
        std::string message = logBuffer;
        // free the buffer
        delete[] logBuffer;
        // throw an exception
        throw std::runtime_error (message);
    }

    // after being liked shaders can be dettached and deleted
    glDetachShader (this->m_programID, vertexShaderID);
    glDetachShader (this->m_programID, fragmentShaderID);

    glDeleteShader (vertexShaderID);
    glDeleteShader (fragmentShaderID);

    // setup uniforms
    this->setupUniforms ();
    // setup attributes too
    this->setupAttributes ();
    // get information from the program, like uniforms, etc
    // support three textures for now
    this->g_Texture0Rotation = glGetUniformLocation (this->m_programID, "g_Texture0Rotation");
    this->g_Texture0Translation = glGetUniformLocation (this->m_programID, "g_Texture0Translation");

    // bind a_TexCoord and a_Position
    this->a_TexCoord = glGetAttribLocation (this->m_programID, "a_TexCoord");
    this->a_Position = glGetAttribLocation (this->m_programID, "a_Position");
}

void CPass::setupAttributes ()
{
    this->addAttribute ("a_TexCoord", GL_FLOAT, 2, &this->a_TexCoord);
    this->addAttribute ("a_Position", GL_FLOAT, 3, &this->a_Position);
}

void CPass::setupUniforms ()
{
    // register all the texture uniforms with correct values
    this->addUniform ("g_Texture0", 0);
    this->addUniform ("g_Texture1", 1);
    this->addUniform ("g_Texture2", 2);
    this->addUniform ("g_Texture3", 3);
    this->addUniform ("g_Texture4", 4);
    this->addUniform ("g_Texture5", 5);
    this->addUniform ("g_Texture6", 6);
    this->addUniform ("g_Texture7", 7);
    // register all the texture sizes required
    this->addUniform ("g_Texture0Resolution", this->m_material->getImage ()->getTexture ()->getResolution ());
    // register the extra texture resolutions
    {
        auto cur = this->m_textures.begin ();
        auto end = this->m_textures.end ();

        for (int index = 1; cur != end; cur ++, index ++)
        {
            std::ostringstream namestream;

            namestream << "g_Texture" << index << "Resolution";

            this->addUniform (namestream.str (), (*cur)->getResolution ());
        }
    }

    // register variables like brightness and alpha with some default value
    this->addUniform ("g_Brightness", 1.0f);
    this->addUniform ("g_UserAlpha", 1.0f);
    // add some external variables
    this->addUniform ("g_Time", &g_Time);
    // add model-view-projection matrix
    this->addUniform ("g_ModelViewProjectionMatrix", &this->m_modelViewProjectionMatrix);
}

void CPass::addAttribute (const std::string& name, GLint type, GLint elements, const GLuint* value)
{
    GLint id = glGetAttribLocation (this->m_programID, name.c_str ());

    if (id == -1)
        return;

    this->m_attribs.emplace_back (
        new AttribEntry (id, name, type, elements, value)
    );
}

template <typename T>
void CPass::addUniform (const std::string& name, UniformType type, T value)
{
    GLint id = glGetUniformLocation (this->m_programID, name.c_str ());

    // parameter not found, can be ignored
    if (id == -1)
        return;

    // build a copy of the value and allocate it somewhere
    T* newValue = new T (value);

    // uniform found, add it to the list
    this->m_uniforms.emplace_back (
        new UniformEntry (id, name, type, newValue)
    );
}

template <typename T>
void CPass::addUniform (const std::string& name, UniformType type, T* value)
{
    // this version is used to reference to system variables so things like g_Time works fine
    GLint id = glGetUniformLocation (this->m_programID, name.c_str ());

    // parameter not found, can be ignored
    if (id == -1)
        return;

    // uniform found, add it to the list
    this->m_uniforms.emplace_back (
        new UniformEntry (id, name, type, value)
    );
}

void CPass::setupTextures ()
{
    auto cur = this->m_pass->getTextures ().begin ();
    auto end = this->m_pass->getTextures ().end ();

    for (int index = 0; cur != end; cur ++, index ++)
    {
        // ignore first texture as that'll be the input of the last pass/image
        if (index == 0)
            continue;

        uint32_t textureSize = 0;

        // get the first texture on the first pass (this one represents the image assigned to this object)
        void* textureData = this->m_material->getImage ()->getContainer ()->readTexture (
            (*cur), &textureSize
        );
        // load a new texture and push it into the list
        this->m_textures.emplace_back (new CTexture (textureData));
    }
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

        // variable not found, can be ignored
        if (vertexVar == nullptr && pixelVar == nullptr)
            continue;

        // if both can be found, ensure they're the correct type
        if (vertexVar != nullptr && pixelVar != nullptr)
        {
            if (vertexVar->getType () != pixelVar->getType ())
                throw std::runtime_error ("Pixel and vertex shader variable types do not match");
        }

        // get one instance of it
        CShaderVariable* var = vertexVar == nullptr ? pixelVar : vertexVar;

        // ensure the shader's and the constant are of the same type
        if ((*cur).second->getType () != var->getType ())
            throw std::runtime_error ("Constant and pixel/vertex variable are not of the same type");

        // now determine the constant's type and register the correct uniform for it
        if ((*cur).second->is <CShaderConstantFloat> ())
            this->addUniform (var->getName (), (*cur).second->as <CShaderConstantFloat> ()->getValue ());
        else if ((*cur).second->is <CShaderConstantInteger> ())
            this->addUniform (var->getName (), (*cur).second->as <CShaderConstantInteger> ()->getValue ());
        else if ((*cur).second->is <CShaderConstantVector3> ())
            this->addUniform (var->getName (), (*cur).second->as <CShaderConstantVector3> ()->getValue ());
    }
}

// define some basic methods for the template
/*template double Core::jsonFindDefault (nlohmann::json& data, const char *key, double defaultValue);*/
void CPass::addUniform (const std::string& name, int value)
{
    this->addUniform (name, UniformType::Integer, value);
}

void CPass::addUniform (const std::string& name, const int* value)
{
    this->addUniform (name, UniformType::Integer, value);
}

void CPass::addUniform (const std::string& name, double value)
{
    this->addUniform (name, UniformType::Double, value);
}

void CPass::addUniform (const std::string& name, const double* value)
{
    this->addUniform (name, UniformType::Double, value);
}

void CPass::addUniform (const std::string& name, float value)
{
    this->addUniform (name, UniformType::Float, value);
}

void CPass::addUniform (const std::string& name, const float* value)
{
    this->addUniform (name, UniformType::Float, value);
}

void CPass::addUniform (const std::string& name, glm::vec2 value)
{
    this->addUniform (name, UniformType::Vector2, value);
}

void CPass::addUniform (const std::string& name, const glm::vec2* value)
{
    this->addUniform (name, UniformType::Vector2, value);
}

void CPass::addUniform (const std::string& name, glm::vec3 value)
{
    this->addUniform (name, UniformType::Vector3, value);
}

void CPass::addUniform (const std::string& name, const glm::vec3* value)
{
    this->addUniform (name, UniformType::Vector3, value);
}

void CPass::addUniform (const std::string& name, glm::vec4 value)
{
    this->addUniform (name, UniformType::Vector4, value);
}

void CPass::addUniform (const std::string& name, const glm::vec4* value)
{
    this->addUniform (name, UniformType::Vector4, value);
}

void CPass::addUniform (const std::string& name, glm::mat4 value)
{
    this->addUniform (name, UniformType::Matrix4, value);
}

void CPass::addUniform (const std::string& name, const glm::mat4* value)
{
    this->addUniform (name, UniformType::Matrix4, value);
}
