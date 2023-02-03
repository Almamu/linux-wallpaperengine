#include "common.h"
#include <sstream>
#include "CPass.h"
#include "WallpaperEngine/Render/CFBO.h"

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableFloat.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableInteger.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector2.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector3.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector4.h"

#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;
using namespace WallpaperEngine::Render::Shaders::Variables;

using namespace WallpaperEngine::Render::Objects::Effects;

extern float g_Time;

CPass::CPass (CMaterial* material, Core::Objects::Images::Materials::CPass* pass) :
    m_material (material),
    m_pass (pass)
{
    this->setupTextures ();
    this->setupShaders ();
    this->setupShaderVariables ();
}


const ITexture* CPass::resolveTexture (const ITexture* expected, int index, const ITexture* previous)
{
    if (expected == nullptr)
    {
        auto it = this->m_fbos.find (index);

        if (it != this->m_fbos.end ())
            expected = (*it).second;
    }

    // first check in the binds and replace it if necessary
    auto it = this->m_material->getMaterial ()->getTextureBinds ().find (index);

    if (it == this->m_material->getMaterial ()->getTextureBinds ().end ())
        return expected;

    // a bind named "previous" is just another way of telling it to use whatever texture there was already
    if ((*it).second->getName () == "previous")
        if (previous == nullptr)
            return expected;
        else
            return previous;
    // the bind actually has a name, search the FBO in the effect and return it
    auto fbo = this->m_material->m_effect->findFBO ((*it).second->getName ());

    // try scene FBOs, these are our last resort, i guess the exception is better than a nullpo
    if (fbo == nullptr)
        return this->m_material->getImage ()->getScene ()->findFBO ((*it).second->getName ());

    return fbo;
}

void CPass::render ()
{
    // set the framebuffer we're drawing to
    glBindFramebuffer (GL_FRAMEBUFFER, this->m_drawTo->getFramebuffer ());

    // set proper viewport based on what we're drawing to
    glViewport (0, 0, this->m_drawTo->getRealWidth (), this->m_drawTo->getRealHeight ());

    // set texture blending
    if (this->m_pass->getBlendingMode () == "translucent")
    {
        glEnable (GL_BLEND);
        glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    else if (this->m_pass->getBlendingMode () == "additive")
    {
        glEnable (GL_BLEND);
        glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
    }
    else if (this->m_pass->getBlendingMode () == "normal")
    {
        glEnable (GL_BLEND);
        glBlendFuncSeparate (GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
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

    if (this->m_pass->getCullingMode () == "nocull")
    {
        glDisable (GL_CULL_FACE);
    }
    else
    {
        glEnable (GL_CULL_FACE);
    }

    if (this->m_pass->getDepthWrite () == "disabled")
    {
        glDepthMask (false);
    }
    else
    {
        glDepthMask (true);
    }

    // use the shader we have registered
    glUseProgram (this->m_programID);

    // maybe we can do this when setting the texture?
    const ITexture* texture = this->resolveTexture (this->m_input, 0, this->m_input);

    uint32_t currentTexture = 0;
    glm::vec2 translation = {0.0f, 0.0f};
    glm::vec4 rotation = {0.0f, 0.0f, 0.0f, 0.0f};

    if (texture->isAnimated () == true)
    {
        // calculate current texture and frame
        double currentRenderTime = fmod (static_cast <double> (g_Time), this->m_material->getImage ()->getAnimationTime ());

        for (const auto& frameCur : texture->getFrames ())
        {
            currentRenderTime -= frameCur->frametime;

            if (currentRenderTime <= 0.0f)
            {
                // frame found, store coordinates and done
                currentTexture = frameCur->frameNumber;

                translation.x = frameCur->x / texture->getTextureWidth (currentTexture);
                translation.y = frameCur->y / texture->getTextureHeight (currentTexture);

                rotation.x = frameCur->width1 / static_cast<float> (texture->getTextureWidth (currentTexture));
                rotation.y = frameCur->width2 / static_cast<float> (texture->getTextureWidth(currentTexture));
                rotation.z = frameCur->height2 / static_cast<float> (texture->getTextureHeight (currentTexture));
                rotation.w = frameCur->height1 / static_cast<float> (texture->getTextureHeight (currentTexture));
                break;
            }
        }
    }

    // first texture is a bit special as we have to take what comes from the chain first
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, texture->getTextureID (currentTexture));

    // continue on the map from the second texture
    if (this->m_finalTextures.empty () == false)
    {
        for (const auto& cur : this->m_finalTextures)
        {
            texture = this->resolveTexture (cur.second, cur.first, this->m_input);

            glActiveTexture (GL_TEXTURE0 + cur.first);
            glBindTexture (GL_TEXTURE_2D, texture->getTextureID (0));
        }
    }

    // add uniforms
    for (const auto& cur : this->m_uniforms)
    {
        UniformEntry* entry = cur.second;

        switch (entry->type)
        {
            case Double:
                glUniform1d (entry->id, *reinterpret_cast <const double*> (entry->value));
                break;
            case Float:
                glUniform1f (entry->id, *reinterpret_cast <const float*> (entry->value));
                break;
            case Integer:
                glUniform1i (entry->id, *reinterpret_cast <const int*> (entry->value));
                break;
            case Vector4:
                glUniform4fv (entry->id, 1, glm::value_ptr (*reinterpret_cast <const glm::vec4*> (entry->value)));
                break;
            case Vector3:
                glUniform3fv (entry->id, 1, glm::value_ptr (*reinterpret_cast <const glm::vec3*> (entry->value)));
                break;
            case Vector2:
                glUniform2fv (entry->id, 1, glm::value_ptr (*reinterpret_cast <const glm::vec2*> (entry->value)));
                break;
            case Matrix4:
                glUniformMatrix4fv (entry->id, 1, GL_FALSE, glm::value_ptr (*reinterpret_cast <const glm::mat4*> (entry->value)));
                break;
        }
    }
    // add reference uniforms
    for (const auto& cur : this->m_referenceUniforms)
    {
        ReferenceUniformEntry* entry = cur.second;

        switch (entry->type)
        {
            case Double:
                glUniform1d (entry->id, *reinterpret_cast <const double*> (*entry->value));
                break;
            case Float:
                glUniform1f (entry->id, *reinterpret_cast <const float*> (*entry->value));
                break;
            case Integer:
                glUniform1i (entry->id, *reinterpret_cast <const int*> (*entry->value));
                break;
            case Vector4:
                glUniform4fv (entry->id, 1, glm::value_ptr (*reinterpret_cast <const glm::vec4*> (*entry->value)));
                break;
            case Vector3:
                glUniform3fv (entry->id, 1, glm::value_ptr (*reinterpret_cast <const glm::vec3*> (*entry->value)));
                break;
            case Vector2:
                glUniform2fv (entry->id, 1, glm::value_ptr (*reinterpret_cast <const glm::vec2*> (*entry->value)));
                break;
            case Matrix4:
                glUniformMatrix4fv (entry->id, 1, GL_FALSE, glm::value_ptr (*reinterpret_cast <const glm::mat4*> (*entry->value)));
                break;
        }
    }

    // used in animations when one of the frames is vertical instead of horizontal
    // rotation with translation = origin and end of the image to display
    if (this->g_Texture0Rotation != -1)
        glUniform4f (this->g_Texture0Rotation, rotation.x, rotation.y, rotation.z, rotation.w);
    // this actually picks the origin point of the image from the atlast
    if (this->g_Texture0Translation != -1)
        glUniform2f (this->g_Texture0Translation, translation.x, translation.y);

    for (const auto& cur : this->m_attribs)
    {
        glEnableVertexAttribArray (cur->id);
        glBindBuffer (GL_ARRAY_BUFFER, *cur->value);
        glVertexAttribPointer (cur->id, cur->elements, cur->type, GL_FALSE, 0, nullptr);

        if (DEBUG)
            glObjectLabel (GL_BUFFER, *cur->value, -1, (
                    "Image " + std::to_string (this->getMaterial ()->getImage ()->getId ()) +
                    " Pass " + this->m_pass->getShader() +
                    " " + cur->name
                ).c_str ()
            );
    }

    // start actual rendering now
    glBindBuffer (GL_ARRAY_BUFFER, this->a_Position);
    glDrawArrays (GL_TRIANGLES, 0, 6);

    // disable vertex attribs array and textures
    for (const auto& cur : this->m_attribs)
        glDisableVertexAttribArray (cur->id);

    // unbind all the used textures
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, 0);

    // continue on the map from the second texture
    if (this->m_finalTextures.empty () == false)
    {
        for (const auto& cur : this->m_finalTextures)
        {
            glActiveTexture (GL_TEXTURE0 + cur.first);
            glBindTexture (GL_TEXTURE_2D, 0);
        }
    }
}

const CMaterial* CPass::getMaterial () const
{
    return this->m_material;
}

void CPass::setDestination (const CFBO* drawTo)
{
    this->m_drawTo = drawTo;
}

void CPass::setInput (const ITexture* input)
{
    this->m_input = input;
}

void CPass::setModelViewProjectionMatrix (const glm::mat4* projection)
{
    this->m_modelViewProjectionMatrix = projection;
}

void CPass::setModelMatrix (glm::mat4 model)
{
    this->m_modelMatrix = model;
}

void CPass::setTexCoord (GLuint texcoord)
{
    this->a_TexCoord = texcoord;
}

void CPass::setPosition (GLuint position)
{
    this->a_Position = position;
}

Core::Objects::Images::Materials::CPass* CPass::getPass ()
{
    return this->m_pass;
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
        std::stringstream buffer;
        buffer << logBuffer << std::endl << "Compiled source code:" << std::endl << shader->getCompiled ();
        // free the buffer
        delete[] logBuffer;
        // throw an exception
        sLog.exception (buffer.str ());
    }

    return shaderID;
}

void CPass::setupShaders ()
{
    // ensure the constants are defined
    const ITexture* texture0 = this->m_material->getImage ()->getTexture ();

    // TODO: THE VALUES ARE THE SAME AS THE ENUMERATION, SO MAYBE IT HAS TO BE SPECIFIED FOR THE TEXTURE 0 OF ALL ELEMENTS?
    if (texture0 != nullptr)
    {
        if (texture0->getFormat () == ITexture::TextureFormat::RG88)
        {
            this->m_pass->insertCombo ("TEX0FORMAT", 8);
        }
        else if (texture0->getFormat () == ITexture::TextureFormat::R8)
        {
            this->m_pass->insertCombo ("TEX0FORMAT", 9);
        }
    }

    // prepare the shaders
    this->m_fragShader = new Render::Shaders::Compiler (
        this->m_material->getImage ()->getContainer (),
        this->m_pass->getShader (),
        Shaders::Compiler::Type_Pixel,
        this->m_pass->getCombos (),
        &m_foundCombos,
        this->m_pass->getTextures (),
        this->m_pass->getConstants ()
    );
    this->m_fragShader->precompile ();
    this->m_vertShader = new Render::Shaders::Compiler (
        this->m_material->getImage ()->getContainer (),
        this->m_pass->getShader (),
        Shaders::Compiler::Type_Vertex,
        this->m_pass->getCombos (),
        &m_foundCombos,
        this->m_pass->getTextures (),
        this->m_pass->getConstants ()
    );
    this->m_vertShader->precompile ();
    this->m_fragShader->precompile ();
    this->m_vertShader->precompile ();

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
        sLog.exception (message);
    }

    if (DEBUG)
    {
        glObjectLabel (GL_PROGRAM, this->m_programID, -1, this->m_pass->getShader ().c_str ());
        glObjectLabel (GL_SHADER, vertexShaderID, -1, (this->m_pass->getShader () + ".vert").c_str ());
        glObjectLabel (GL_SHADER, fragmentShaderID, -1, (this->m_pass->getShader () + ".frag").c_str ());
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
}

void CPass::setupAttributes ()
{
    this->addAttribute ("a_TexCoord", GL_FLOAT, 2, &this->a_TexCoord);
    this->addAttribute ("a_Position", GL_FLOAT, 3, &this->a_Position);
}

void CPass::setupUniforms ()
{
    // resolve the main texture
    const ITexture* texture = this->resolveTexture (this->m_material->getImage ()->getTexture (), 0);
    // register all the texture uniforms with correct values
    this->addUniform ("g_Texture0", 0);
    this->addUniform ("g_Texture1", 1);
    this->addUniform ("g_Texture2", 2);
    this->addUniform ("g_Texture3", 3);
    this->addUniform ("g_Texture4", 4);
    this->addUniform ("g_Texture5", 5);
    this->addUniform ("g_Texture6", 6);
    this->addUniform ("g_Texture7", 7);
    this->addUniform ("g_Texture0Resolution", texture->getResolution ());

    // do the real, final texture setup for the whole process
    {
        auto cur = this->m_textures.begin ();
        auto end = this->m_textures.end ();
        auto fragCur = this->m_fragShader->getTextures ().begin ();
        auto fragEnd = this->m_fragShader->getTextures ().end ();
        auto vertCur = this->m_vertShader->getTextures ().begin ();
        auto vertEnd = this->m_vertShader->getTextures ().end ();
        auto bindCur = this->m_material->getMaterial ()->getTextureBinds ().begin ();
        auto bindEnd = this->m_material->getMaterial ()->getTextureBinds ().end ();

        int index = 1;

        // technically m_textures should have the right amount of textures
        // but better be safe than sorry
        while (bindCur != bindEnd || cur != end || fragCur != fragEnd || vertCur != vertEnd)
        {
            if (bindCur != bindEnd)
            {
                this->m_finalTextures.insert (std::make_pair ((*bindCur).first, nullptr));
                bindCur ++;
            }

            if (cur != end)
            {
                if ((*cur) != nullptr)
                    this->m_finalTextures.insert (std::make_pair (index, *cur));

                index ++;
                cur ++;
            }

            if (fragCur != fragEnd)
            {
                std::string textureName = (*fragCur).second;

                try
                {
                    // resolve the texture first
                    const ITexture* textureRef = nullptr;

                    if (textureName.find ("_rt_") == 0)
                    {
                        textureRef = this->getMaterial ()->getEffect ()->findFBO (textureName);

                        if (textureRef == nullptr)
                            textureRef = this->getMaterial ()->getImage ()->getScene ()->findFBO (textureName);
                    }
                    else
                        textureRef = this->getMaterial ()->getImage ()->getScene ()->getContext ()->resolveTexture (textureName);

                    this->m_finalTextures.insert (std::make_pair ((*fragCur).first, textureRef));
                }
                catch (std::runtime_error& ex)
                {
                    sLog.error ("Cannot resolve texture ", textureName, " for fragment shader ", ex.what ());
                }

                fragCur ++;
            }

            if (vertCur != vertEnd)
            {
                std::string textureName = (*vertCur).second;

                try
                {
                    // resolve the texture first
                    const ITexture* textureRef = nullptr;

                    if (textureName.find ("_rt_") == 0)
                    {
                        textureRef = this->getMaterial ()->getEffect ()->findFBO (textureName);

                        if (textureRef == nullptr)
                            textureRef = this->getMaterial ()->getImage ()->getScene ()->findFBO (textureName);
                    }
                    else
                        textureRef = this->getMaterial ()->getImage ()->getScene ()->getContext ()->resolveTexture (textureName);

                    this->m_finalTextures.insert (std::make_pair ((*vertCur).first, textureRef));
                }
                catch (std::runtime_error& ex)
                {
                    sLog.error ("Cannot resolve texture ", textureName, " for vertex shader ", ex.what ());
                }

                vertCur ++;
            }
        }
    }

    for (const auto& cur : this->m_finalTextures)
    {
        std::ostringstream namestream;

        namestream << "g_Texture" << cur.first << "Resolution";

        texture = this->resolveTexture (cur.second, cur.first, texture);
        this->addUniform (namestream.str (), texture->getResolution ());
    }

    auto projection = this->getMaterial ()->getImage ()->getScene ()->getScene ()->getOrthogonalProjection ();

    // register variables like brightness and alpha with some default value
    this->addUniform ("g_Brightness", this->m_material->getImage ()->getImage ()->getBrightness ());
    this->addUniform ("g_UserAlpha", this->m_material->getImage ()->getImage ()->getAlpha ());
    this->addUniform ("g_Alpha", this->m_material->getImage ()->getImage ()->getAlpha ());
    this->addUniform ("g_Color", this->m_material->getImage ()->getImage ()->getColor ());
    // TODO: VALIDATE THAT G_COMPOSITECOLOR REALLY COMES FROM THIS ONE
    this->addUniform ("g_CompositeColor", this->m_material->getImage ()->getImage ()->getColor ());
    // add some external variables
    this->addUniform ("g_Time", &g_Time);
    // add model-view-projection matrix
    this->addUniform ("g_ModelViewProjectionMatrix", &this->m_modelViewProjectionMatrix);
    this->addUniform ("g_PointerPosition", this->m_material->getImage ()->getScene ()->getMousePosition ());
    this->addUniform ("g_PointerPositionLast", this->m_material->getImage ()->getScene ()->getMousePositionLast ());
    this->addUniform ("g_EffectTextureProjectionMatrix", glm::mat4(1.0));
    this->addUniform ("g_EffectTextureProjectionMatrixInverse", glm::mat4(1.0));
    this->addUniform ("g_TexelSize", glm::vec2 (1.0 / projection->getWidth (), 1.0 / projection->getHeight ()));
    this->addUniform ("g_TexelSizeHalf", glm::vec2 (0.5 / projection->getWidth (), 0.5 / projection->getHeight ()));
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
    this->m_uniforms.insert (
        std::make_pair (name, new UniformEntry (id, name, type, newValue))
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
    this->m_uniforms.insert (
        std::make_pair (name, new UniformEntry (id, name, type, value))
    );
}

template <typename T>
void CPass::addUniform (const std::string& name, UniformType type, T** value)
{
    // this version is used to reference to system variables so things like g_Time works fine
    GLint id = glGetUniformLocation (this->m_programID, name.c_str ());

    // parameter not found, can be ignored
    if (id == -1)
        return;

    // uniform found, add it to the list
    this->m_referenceUniforms.insert (
        std::make_pair (name, new ReferenceUniformEntry (id, name, type, reinterpret_cast <const void**> (value)))
    );
}


void CPass::setupTextures ()
{
    auto cur = this->m_pass->getTextures ().begin ();
    auto end = this->m_pass->getTextures ().end ();

    for (int index = 0; cur != end; cur ++, index ++)
    {
        // ignore first texture as that'll be the input of the last pass/image (unless the image is an FBO)
        if (index == 0)
            continue;

        if ((*cur).find ("_rt_") == 0)
        {
            const CFBO* fbo = this->m_material->m_effect->findFBO ((*cur));

            if (fbo == nullptr)
                fbo = this->m_material->getImage ()->getScene ()->findFBO ((*cur));

            if (fbo != nullptr)
            {
                this->m_fbos.insert (std::make_pair (index, fbo));
                this->m_textures.emplace_back (
                    fbo
                );
            }
            // _rt_texture
        }
        else
        {
            if ((*cur) == "")
            {
                this->m_textures.emplace_back (nullptr);
            }
            else
            {
                this->m_textures.emplace_back (
                    this->m_material->getImage ()->getScene ()->getContext ()->resolveTexture ((*cur))
                );
            }
        }
    }
}

void CPass::setupShaderVariables ()
{
    // find variables in the shaders and set the value with the constants if possible
    for (const auto& cur : this->m_pass->getConstants ())
    {
        CShaderVariable* vertexVar = this->m_vertShader->findParameter (cur.first);
        CShaderVariable* pixelVar = this->m_fragShader->findParameter (cur.first);

        // variable not found, can be ignored
        if (vertexVar == nullptr && pixelVar == nullptr)
            continue;

        // if both can be found, ensure they're the correct type
        /*if (vertexVar != nullptr && pixelVar != nullptr)
        {
            if (vertexVar->getType () != pixelVar->getType ())
                throw std::runtime_error ("Pixel and vertex shader variable types do not match");
        }*/

        // get one instance of it
        CShaderVariable* var = vertexVar == nullptr ? pixelVar : vertexVar;

        // ensure the shader's and the constant are of the same type
        if (cur.second->getType () != var->getType ())
        {
            // there's situations where this type mismatch is actually expected
            // integers and floats are equivalent, this could be detected at load time
            // but that'd mean to compile the shader in the load, and not on the render stage
            // so take into account these conversions here

            if (cur.second->is <CShaderConstantFloat> () == true && var->is <CShaderVariableInteger> () == true)
            {
                // create an integer value from a float
                this->addUniform (var->getName (), static_cast <int> (*cur.second->as <CShaderConstantFloat> ()->getValue ()));
            }
            else if (cur.second->is <CShaderConstantInteger> () == true && var->is <CShaderVariableFloat> () == true)
            {
                // create a float value from an integer
                this->addUniform (var->getName (), static_cast <float> (*cur.second->as <CShaderConstantInteger> ()->getValue ()));
            }
            else if (cur.second->is <CShaderConstantVector4> () == true && var->is <CShaderVariableVector2> () == true)
            {
                CShaderConstantVector4* val = cur.second->as <CShaderConstantVector4> ();

                // create a new vector2 with the first two values
                this->addUniform (var->getName (), {val->getValue ()->x, val->getValue ()->y});
            }
            else if (cur.second->is <CShaderConstantVector4> () == true && var->is <CShaderVariableVector3> () == true)
            {
                CShaderConstantVector4* val = cur.second->as <CShaderConstantVector4> ();

                this->addUniform (var->getName (), {val->getValue ()->x, val->getValue ()->y, val->getValue ()->z});
            }
            else
            {
                sLog.exception (
                    "Constant ",
                    cur.first,
                    " type does not match pixel/vertex shader variable and cannot be converted (",
                    cur.second->getType (),
                    " to ",
                    var->getType ()
                );
            }
        }
        else
        {
            // now determine the constant's type and register the correct uniform for it
            if (cur.second->is <CShaderConstantFloat> ())
                this->addUniform (var->getName (), cur.second->as <CShaderConstantFloat> ()->getValue ());
            else if (cur.second->is <CShaderConstantInteger> ())
                this->addUniform (var->getName (), cur.second->as <CShaderConstantInteger> ()->getValue ());
            else if (cur.second->is <CShaderConstantVector4> ())
                this->addUniform (var->getName (), cur.second->as <CShaderConstantVector4> ()->getValue ());
        }
    }

    for (const auto& cur : this->m_vertShader->getParameters ())
    {
        if (this->m_uniforms.find (cur->getName ()) != this->m_uniforms.end ())
            continue;

        if (cur->is <CShaderVariableFloat> ())
            this->addUniform (cur->getName (), const_cast <float*> (reinterpret_cast <const float*> (cur->as <CShaderVariableFloat> ()->getValue ())));
        else if (cur->is <CShaderVariableInteger> ())
            this->addUniform (cur->getName (), const_cast <int*> (reinterpret_cast <const int*> (cur->as <CShaderVariableInteger> ()->getValue ())));
        else if (cur->is <CShaderVariableVector2> ())
            this->addUniform (cur->getName (), const_cast <glm::vec2*> (reinterpret_cast <const glm::vec2*> (cur->as <CShaderVariableVector2> ()->getValue ())));
        else if (cur->is <CShaderVariableVector3> ())
            this->addUniform (cur->getName (), const_cast <glm::vec3*> (reinterpret_cast <const glm::vec3*> (cur->as <CShaderVariableVector3> ()->getValue ())));
        else if (cur->is <CShaderVariableVector4> ())
            this->addUniform (cur->getName (), const_cast <glm::vec4*> (reinterpret_cast <const glm::vec4*> (cur->as <CShaderVariableVector4> ()->getValue ())));
    }

    for (const auto& cur : this->m_fragShader->getParameters ())
    {
        if (this->m_uniforms.find (cur->getName ()) != this->m_uniforms.end ())
            continue;

        if (cur->is <CShaderVariableFloat> ())
            this->addUniform (cur->getName (), const_cast <float*> (reinterpret_cast <const float*> (cur->as <CShaderVariableFloat> ()->getValue ())));
        else if (cur->is <CShaderVariableInteger> ())
            this->addUniform (cur->getName (), const_cast <int*> (reinterpret_cast <const int*> (cur->as <CShaderVariableInteger> ()->getValue ())));
        else if (cur->is <CShaderVariableVector2> ())
            this->addUniform (cur->getName (), const_cast <glm::vec2*> (reinterpret_cast <const glm::vec2*> (cur->as <CShaderVariableVector2> ()->getValue ())));
        else if (cur->is <CShaderVariableVector3> ())
            this->addUniform (cur->getName (), const_cast <glm::vec3*> (reinterpret_cast <const glm::vec3*> (cur->as <CShaderVariableVector3> ()->getValue ())));
        else if (cur->is <CShaderVariableVector4> ())
            this->addUniform (cur->getName (), const_cast <glm::vec4*> (reinterpret_cast <const glm::vec4*> (cur->as <CShaderVariableVector4> ()->getValue ())));
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

void CPass::addUniform (const std::string& name, const int** value)
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

void CPass::addUniform (const std::string& name, const double** value)
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

void CPass::addUniform (const std::string& name, const float** value)
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

void CPass::addUniform (const std::string& name, const glm::vec2** value)
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

void CPass::addUniform (const std::string& name, const glm::vec3** value)
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

void CPass::addUniform (const std::string& name, const glm::vec4** value)
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

void CPass::addUniform (const std::string& name, const glm::mat4** value)
{
    this->addUniform (name, UniformType::Matrix4, value);
}
