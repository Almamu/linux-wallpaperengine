#include "CPass.h"
#include "WallpaperEngine/Render/CFBO.h"
#include <sstream>

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
#include "WallpaperEngine/Logging/CLog.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;
using namespace WallpaperEngine::Render::Shaders::Variables;

using namespace WallpaperEngine::Render::Objects::Effects;

extern float g_Time;
extern float g_Daytime;

CPass::CPass (CMaterial* material, const Core::Objects::Images::Materials::CPass* pass) :
    Helpers::CContextAware (material),
    m_material (material),
    m_pass (pass),
    m_blendingmode (pass->getBlendingMode ()) {
    this->setupTextures ();
    this->setupShaders ();
    this->setupShaderVariables ();
}

const ITexture* CPass::resolveTexture (const ITexture* expected, int index, const ITexture* previous) {
    if (expected == nullptr) {
        const auto it = this->m_fbos.find (index);

        if (it != this->m_fbos.end ())
            expected = it->second;
    }

    // first check in the binds and replace it if necessary
    const auto it = this->m_material->getMaterial ()->getTextureBinds ().find (index);

    if (it == this->m_material->getMaterial ()->getTextureBinds ().end ())
        return expected;

    // a bind named "previous" is just another way of telling it to use whatever texture there was already
    if (it->second->getName () == "previous")
        return previous ?: expected;

    // the bind actually has a name, search the FBO in the effect and return it
    const auto fbo = this->m_material->m_effect->findFBO (it->second->getName ());

    // try scene FBOs, these are our last resort, i guess the exception is better than a nullpo
    if (fbo == nullptr)
        return this->m_material->getImage ()->getScene ()->findFBO (it->second->getName ());

    return fbo;
}

void CPass::setupRenderFramebuffer () {
    // set the framebuffer we're drawing to
    glBindFramebuffer (GL_FRAMEBUFFER, this->m_drawTo->getFramebuffer ());

    // set proper viewport based on what we're drawing to
    glViewport (0, 0, this->m_drawTo->getRealWidth (), this->m_drawTo->getRealHeight ());

    // set texture blending
    if (this->m_pass->getBlendingMode () == "translucent") {
        glEnable (GL_BLEND);
        glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else if (this->m_pass->getBlendingMode () == "additive") {
        glEnable (GL_BLEND);
        glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
    } else if (this->m_pass->getBlendingMode () == "normal") {
        glEnable (GL_BLEND);
        glBlendFuncSeparate (GL_ONE, GL_ZERO, GL_ONE, GL_ZERO);
    } else {
        glDisable (GL_BLEND);
    }

    // set depth testing
    if (this->m_pass->getDepthTest () == "disabled") {
        glDisable (GL_DEPTH_TEST);
    } else {
        glEnable (GL_DEPTH_TEST);
    }

    if (this->m_pass->getCullingMode () == "nocull") {
        glDisable (GL_CULL_FACE);
    } else {
        glEnable (GL_CULL_FACE);
    }

    if (this->m_pass->getDepthWrite () == "disabled") {
        glDepthMask (false);
    } else {
        glDepthMask (true);
    }
}

void CPass::setupRenderTexture () {
    // use the shader we have registered
    glUseProgram (this->m_programID);

    // maybe we can do this when setting the texture?
    const ITexture* texture = this->resolveTexture (this->m_input, 0, this->m_input);

    uint32_t currentTexture = 0;
    glm::vec2 translation = {0.0f, 0.0f};
    glm::vec4 rotation = {0.0f, 0.0f, 0.0f, 0.0f};

    if (texture->isAnimated ()) {
        // calculate current texture and frame
        double currentRenderTime = fmod (static_cast<double> (this->getContext ().getDriver ().getRenderTime ()),
                                         this->m_material->getImage ()->getAnimationTime ());

        for (const auto& frameCur : texture->getFrames ()) {
            currentRenderTime -= frameCur->frametime;

            if (currentRenderTime <= 0.0f) {
                // frame found, store coordinates and done
                currentTexture = frameCur->frameNumber;

                translation.x = frameCur->x / texture->getTextureWidth (currentTexture);
                translation.y = frameCur->y / texture->getTextureHeight (currentTexture);

                rotation.x = frameCur->width1 / static_cast<float> (texture->getTextureWidth (currentTexture));
                rotation.y = frameCur->width2 / static_cast<float> (texture->getTextureWidth (currentTexture));
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
    if (!this->m_finalTextures.empty ()) {
        for (const auto& [index, expectedTexture] : this->m_finalTextures) {
            texture = this->resolveTexture (expectedTexture, index, this->m_input);

            glActiveTexture (GL_TEXTURE0 + index);
            glBindTexture (GL_TEXTURE_2D, texture->getTextureID (0));
        }
    }

    // used in animations when one of the frames is vertical instead of horizontal
    // rotation with translation = origin and end of the image to display
    if (this->g_Texture0Rotation != -1)
        glUniform4f (this->g_Texture0Rotation, rotation.x, rotation.y, rotation.z, rotation.w);
    // this actually picks the origin point of the image from the atlast
    if (this->g_Texture0Translation != -1)
        glUniform2f (this->g_Texture0Translation, translation.x, translation.y);
}

void CPass::setupRenderReferenceUniforms () {
    // add reference uniforms
    for (const auto& [name, value] : this->m_referenceUniforms) {
        switch (value->type) {
            case Double: glUniform1d (value->id, *static_cast<const double*> (*value->value)); break;
            case Float: glUniform1f (value->id, *static_cast<const float*> (*value->value)); break;
            case Integer: glUniform1i (value->id, *static_cast<const int*> (*value->value)); break;
            case Vector4:
                glUniform4fv (value->id, 1, glm::value_ptr (*static_cast<const glm::vec4*> (*value->value)));
                break;
            case Vector3:
                glUniform3fv (value->id, 1, glm::value_ptr (*static_cast<const glm::vec3*> (*value->value)));
                break;
            case Vector2:
                glUniform2fv (value->id, 1, glm::value_ptr (*static_cast<const glm::vec2*> (*value->value)));
                break;
            case Matrix4:
                glUniformMatrix4fv (value->id, 1, GL_FALSE,
                                    glm::value_ptr (*static_cast<const glm::mat4*> (*value->value)));
                break;
            case Matrix3:
                glUniformMatrix3fv (value->id, 1, GL_FALSE,
                                    glm::value_ptr (*static_cast<const glm::mat3*> (*value->value)));
                break;
        }
    }
}

void CPass::setupRenderUniforms () {
    // add uniforms
    for (const auto& [name, value] : this->m_uniforms) {
        switch (value->type) {
            case Double: glUniform1dv (value->id, value->count, static_cast<const double*> (value->value)); break;
            case Float: glUniform1fv (value->id, value->count, static_cast<const float*> (value->value)); break;
            case Integer: glUniform1iv (value->id, value->count, static_cast<const int*> (value->value)); break;
            // TODO: THESE MIGHT NEED SPECIAL TREATMENT? IDK ONLY SUPPORT 1 FOR NOW
            case Vector4:
                glUniform4fv (value->id, 1, glm::value_ptr (*static_cast<const glm::vec4*> (value->value)));
                break;
            case Vector3:
                glUniform3fv (value->id, 1, glm::value_ptr (*static_cast<const glm::vec3*> (value->value)));
                break;
            case Vector2:
                glUniform2fv (value->id, 1, glm::value_ptr (*static_cast<const glm::vec2*> (value->value)));
                break;
            case Matrix4:
                glUniformMatrix4fv (value->id, 1, GL_FALSE,
                                    glm::value_ptr (*static_cast<const glm::mat4*> (value->value)));
                break;
            case Matrix3:
                glUniformMatrix3fv (value->id, 1, GL_FALSE,
                                    glm::value_ptr (*static_cast<const glm::mat3*> (value->value)));
                break;
        }
    }
}

void CPass::setupRenderAttributes () {
    for (const auto& cur : this->m_attribs) {
        glEnableVertexAttribArray (cur->id);
        glBindBuffer (GL_ARRAY_BUFFER, *cur->value);
        glVertexAttribPointer (cur->id, cur->elements, cur->type, GL_FALSE, 0, nullptr);

#if !NDEBUG
        glObjectLabel (GL_BUFFER, *cur->value, -1,
                       ("Image " + std::to_string (this->getMaterial ()->getImage ()->getId ()) + " Pass " +
                        this->m_pass->getShader () + " " + cur->name)
                           .c_str ());
#endif /* DEBUG */
    }
}

void CPass::renderGeometry () const {
    // start actual rendering now
    glBindBuffer (GL_ARRAY_BUFFER, this->a_Position);
    glDrawArrays (GL_TRIANGLES, 0, 6);
}

void CPass::cleanupRenderSetup () {
    // disable vertex attribs array and textures
    for (const auto& cur : this->m_attribs)
        glDisableVertexAttribArray (cur->id);

    // unbind all the used textures
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, 0);

    // continue on the map from the second texture
    if (!this->m_finalTextures.empty ()) {
        for (const auto& [index, _] : this->m_finalTextures) {
            glActiveTexture (GL_TEXTURE0 + index);
            glBindTexture (GL_TEXTURE_2D, 0);
        }
    }
}

void CPass::render () {
    this->setupRenderFramebuffer ();
    this->setupRenderTexture ();
    this->setupRenderUniforms ();
    this->setupRenderReferenceUniforms ();
    this->setupRenderAttributes ();
    this->renderGeometry ();
    this->cleanupRenderSetup ();
}

const CMaterial* CPass::getMaterial () const {
    return this->m_material;
}

void CPass::setDestination (const CFBO* drawTo) {
    this->m_drawTo = drawTo;
}

void CPass::setInput (const ITexture* input) {
    this->m_input = input;
}

void CPass::setModelViewProjectionMatrix (const glm::mat4* projection) {
    this->m_modelViewProjectionMatrix = projection;
}

void CPass::setModelViewProjectionMatrixInverse (const glm::mat4* projection) {
    this->m_modelViewProjectionMatrixInverse = projection;
}

void CPass::setModelMatrix (const glm::mat4* model) {
    this->m_modelMatrix = model;
}

void CPass::setViewProjectionMatrix (const glm::mat4* viewProjection) {
    this->m_viewProjectionMatrix = viewProjection;
}

void CPass::setBlendingMode (std::string blendingmode) {
    this->m_blendingmode = blendingmode;
}

const std::string& CPass::getBlendingMode () const {
    return this->m_blendingmode;
}

void CPass::setTexCoord (GLuint texcoord) {
    this->a_TexCoord = texcoord;
}

void CPass::setPosition (GLuint position) {
    this->a_Position = position;
}

const Core::Objects::Images::Materials::CPass* CPass::getPass () const {
    return this->m_pass;
}

Render::Shaders::CShader* CPass::getShader () const {
    return this->m_shader;
}

GLuint CPass::compileShader (const char* shader, GLuint type) {
    // reserve shaders in OpenGL
    const GLuint shaderID = glCreateShader (type);

    glShaderSource (shaderID, 1, &shader, nullptr);
    glCompileShader (shaderID);

    GLint result = GL_FALSE;
    int infoLogLength = 0;

    // ensure the vertex shader was correctly compiled
    glGetShaderiv (shaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv (shaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0) {
        char* logBuffer = new char [infoLogLength + 1];
        // ensure logBuffer ends with a \0
        memset (logBuffer, 0, infoLogLength + 1);
        // get information about the error
        glGetShaderInfoLog (shaderID, infoLogLength, nullptr, logBuffer);
        // throw an exception about the issue
        std::stringstream buffer;
        buffer << logBuffer << std::endl << "Compiled source code:" << std::endl << shader;
        // free the buffer
        delete [] logBuffer;
        // throw an exception
        sLog.exception (buffer.str ());
    }

    return shaderID;
}

void CPass::setupShaders () {
    // ensure the constants are defined
    const ITexture* texture0 = this->m_material->getImage ()->getTexture ();

    // TODO: THE VALUES ARE THE SAME AS THE ENUMERATION, SO MAYBE IT HAS TO BE SPECIFIED FOR THE TEXTURE 0 OF ALL
    // ELEMENTS?
    if (texture0 != nullptr) {
        if (texture0->getFormat () == ITexture::TextureFormat::RG88) {
            this->m_foundCombos.insert(std::pair("TEX0FORMAT", 8));
        } else if (texture0->getFormat () == ITexture::TextureFormat::R8) {
            this->m_foundCombos.insert (std::pair("TEX0FORMAT", 9));
        }
    }

    this->m_shader = new Render::Shaders::CShader (
        this->getMaterial ()->getImage ()->getContainer (), this->m_pass->getShader (), this->m_pass->getCombos (),
        this->m_pass->getTextures (), this->m_pass->getConstants ()
    );

    std::string fragmentCode = this->m_shader->fragment ();
    std::string vertexCode = this->m_shader->vertex ();

    // compile the shaders
    const GLuint vertexShaderID = compileShader (vertexCode.c_str (), GL_VERTEX_SHADER);
    const GLuint fragmentShaderID = compileShader (fragmentCode.c_str (), GL_FRAGMENT_SHADER);
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

    if (infoLogLength > 0) {
        char* logBuffer = new char [infoLogLength + 1];
        // ensure logBuffer ends with a \0
        memset (logBuffer, 0, infoLogLength + 1);
        // get information about the error
        glGetProgramInfoLog (this->m_programID, infoLogLength, nullptr, logBuffer);
        // throw an exception about the issue
        const std::string message = logBuffer;
        // free the buffer
        delete [] logBuffer;
        // throw an exception
        sLog.exception (message);
    }

#if !NDEBUG
    glObjectLabel (GL_PROGRAM, this->m_programID, -1, this->m_pass->getShader ().c_str ());
    glObjectLabel (GL_SHADER, vertexShaderID, -1, (this->m_pass->getShader () + ".vert").c_str ());
    glObjectLabel (GL_SHADER, fragmentShaderID, -1, (this->m_pass->getShader () + ".frag").c_str ());
#endif /* DEBUG */

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

void CPass::setupAttributes () {
    this->addAttribute ("a_TexCoord", GL_FLOAT, 2, &this->a_TexCoord);
    this->addAttribute ("a_Position", GL_FLOAT, 3, &this->a_Position);
}

void CPass::setupTextureUniforms () {
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
    auto cur = this->m_textures.begin ();
    const auto end = this->m_textures.end ();
    auto fragCur = this->m_shader->getFragmentTextures ().begin ();
    const auto fragEnd = this->m_shader->getFragmentTextures ().end ();
    auto vertCur = this->m_shader->getVertexTextures ().begin ();
    const auto vertEnd = this->m_shader->getVertexTextures ().end ();
    auto bindCur = this->m_material->getMaterial ()->getTextureBinds ().begin ();
    const auto bindEnd = this->m_material->getMaterial ()->getTextureBinds ().end ();

    int index = 1;

    // technically m_textures should have the right amount of textures
    // but better be safe than sorry
    while (bindCur != bindEnd || cur != end || fragCur != fragEnd || vertCur != vertEnd) {
        if (bindCur != bindEnd) {
            this->m_finalTextures [bindCur->first] = nullptr;
            ++bindCur;
        }

        if (cur != end) {
            if ((*cur) != nullptr)
                this->m_finalTextures [index] = *cur;

            index++;
            ++cur;
        }

        if (fragCur != fragEnd) {
            std::string textureName = fragCur->second;

            try {
                // resolve the texture first
                const ITexture* textureRef;

                if (textureName.find ("_rt_") == 0 || textureName.find ("_alias_") == 0) {
                    textureRef = this->getMaterial ()->getEffect ()->findFBO (textureName);

                    if (textureRef == nullptr)
                        textureRef = this->getMaterial ()->getImage ()->getScene ()->findFBO (textureName);
                } else
                    textureRef = this->getContext ().resolveTexture (textureName);

                // ensure there's no texture in that slot already, shader textures are defaults in case nothing is
                // there
                if (this->m_finalTextures.find (fragCur->first) == this->m_finalTextures.end ())
                    this->m_finalTextures [fragCur->first] = textureRef;
            } catch (std::runtime_error& ex) {
                sLog.error ("Cannot resolve texture ", textureName, " for fragment shader ", ex.what ());
            }

            ++fragCur;
        }

        if (vertCur != vertEnd) {
            std::string textureName = vertCur->second;

            try {
                // resolve the texture first
                const ITexture* textureRef;

                if (textureName.find ("_rt_") == 0) {
                    textureRef = this->getMaterial ()->getEffect ()->findFBO (textureName);

                    if (textureRef == nullptr)
                        textureRef = this->getMaterial ()->getImage ()->getScene ()->findFBO (textureName);
                } else
                    textureRef = this->getContext ().resolveTexture (textureName);

                // ensure there's no texture in that slot already, shader textures are defaults in case nothing is
                // there
                if (this->m_finalTextures.find (vertCur->first) == this->m_finalTextures.end ())
                    this->m_finalTextures [vertCur->first] = textureRef;
            } catch (std::runtime_error& ex) {
                sLog.error ("Cannot resolve texture ", textureName, " for vertex shader ", ex.what ());
            }

            ++vertCur;
        }
    }

    for (const auto& [textureIndex, expectedTexture] : this->m_finalTextures) {
        std::ostringstream namestream;

        namestream << "g_Texture" << textureIndex << "Resolution";

        texture = this->resolveTexture (expectedTexture, textureIndex, texture);
        this->addUniform (namestream.str (), texture->getResolution ());
    }
}

void CPass::setupUniforms () {
    this->setupTextureUniforms ();

    const auto projection = this->getMaterial ()->getImage ()->getScene ()->getScene ()->getOrthogonalProjection ();

    // lighting variables
    this->addUniform ("g_LightAmbientColor",
                      this->m_material->getImage ()->getScene ()->getScene ()->getAmbientColor ());
    this->addUniform ("g_LightSkylightColor",
                      this->m_material->getImage ()->getScene ()->getScene ()->getSkylightColor ());
    // register variables like brightness and alpha with some default value
    this->addUniform ("g_Brightness", this->m_material->getImage ()->getImage ()->getBrightness ());
    this->addUniform ("g_UserAlpha", this->m_material->getImage ()->getImage ()->getAlpha ());
    this->addUniform ("g_Alpha", this->m_material->getImage ()->getImage ()->getAlpha ());
    this->addUniform ("g_Color", this->m_material->getImage ()->getImage ()->getColor ());
    this->addUniform ("g_Color4", glm::vec4 (this->m_material->getImage ()->getImage ()->getColor (), 1));
    // TODO: VALIDATE THAT G_COMPOSITECOLOR REALLY COMES FROM THIS ONE
    this->addUniform ("g_CompositeColor", this->m_material->getImage ()->getImage ()->getColor ());
    // add some external variables
    this->addUniform ("g_Time", &g_Time);
    this->addUniform ("g_Daytime", &g_Daytime);
    // add model-view-projection matrix
    this->addUniform ("g_ModelViewProjectionMatrixInverse", &this->m_modelViewProjectionMatrixInverse);
    this->addUniform ("g_ModelViewProjectionMatrix", &this->m_modelViewProjectionMatrix);
    this->addUniform ("g_ModelMatrix", &this->m_modelMatrix);
    this->addUniform ("g_NormalModelMatrix", glm::identity<glm::mat3> ());
    this->addUniform ("g_ViewProjectionMatrix", &this->m_viewProjectionMatrix);
    this->addUniform ("g_PointerPosition", this->m_material->getImage ()->getScene ()->getMousePosition ());
    this->addUniform ("g_PointerPositionLast", this->m_material->getImage ()->getScene ()->getMousePositionLast ());
    this->addUniform ("g_EffectTextureProjectionMatrix", glm::mat4 (1.0));
    this->addUniform ("g_EffectTextureProjectionMatrixInverse", glm::mat4 (1.0));
    this->addUniform ("g_TexelSize", glm::vec2 (1.0 / projection->getWidth (), 1.0 / projection->getHeight ()));
    this->addUniform ("g_TexelSizeHalf", glm::vec2 (0.5 / projection->getWidth (), 0.5 / projection->getHeight ()));
    this->addUniform ("g_AudioSpectrum16Left",
                      this->getMaterial ()->getImage ()->getScene ()->getAudioContext ().getRecorder ().audio16, 16);
    this->addUniform ("g_AudioSpectrum16Right",
                      this->getMaterial ()->getImage ()->getScene ()->getAudioContext ().getRecorder ().audio16, 16);
    this->addUniform ("g_AudioSpectrum32Left",
                      this->getMaterial ()->getImage ()->getScene ()->getAudioContext ().getRecorder ().audio32, 32);
    this->addUniform ("g_AudioSpectrum32Right",
                      this->getMaterial ()->getImage ()->getScene ()->getAudioContext ().getRecorder ().audio32, 32);
    this->addUniform ("g_AudioSpectrum64Left",
                      this->getMaterial ()->getImage ()->getScene ()->getAudioContext ().getRecorder ().audio64, 64);
    this->addUniform ("g_AudioSpectrum64Right",
                      this->getMaterial ()->getImage ()->getScene ()->getAudioContext ().getRecorder ().audio64, 64);
}

void CPass::addAttribute (const std::string& name, GLint type, GLint elements, const GLuint* value) {
    const GLint id = glGetAttribLocation (this->m_programID, name.c_str ());

    if (id == -1)
        return;

    this->m_attribs.emplace_back (new AttribEntry (id, name, type, elements, value));
}

template <typename T> void CPass::addUniform (const std::string& name, UniformType type, T value) {
    GLint id = glGetUniformLocation (this->m_programID, name.c_str ());

    // parameter not found, can be ignored
    if (id == -1)
        return;

    // build a copy of the value and allocate it somewhere
    T* newValue = new T (value);

    // uniform found, add it to the list
    this->m_uniforms.insert (std::make_pair (name, new UniformEntry (id, name, type, newValue, 1)));
}

template <typename T> void CPass::addUniform (const std::string& name, UniformType type, T* value, int count) {
    // this version is used to reference to system variables so things like g_Time works fine
    GLint id = glGetUniformLocation (this->m_programID, name.c_str ());

    // parameter not found, can be ignored
    if (id == -1)
        return;

    // uniform found, add it to the list
    this->m_uniforms.insert (std::make_pair (name, new UniformEntry (id, name, type, value, count)));
}

template <typename T> void CPass::addUniform (const std::string& name, UniformType type, T** value) {
    // this version is used to reference to system variables so things like g_Time works fine
    const GLint id = glGetUniformLocation (this->m_programID, name.c_str ());

    // parameter not found, can be ignored
    if (id == -1)
        return;

    // uniform found, add it to the list
    this->m_referenceUniforms.insert (
        std::make_pair (name, new ReferenceUniformEntry (id, name, type, reinterpret_cast<const void**> (value))));
}

void CPass::setupTextures () {
    auto cur = this->m_pass->getTextures ().begin ();
    const auto end = this->m_pass->getTextures ().end ();

    for (int index = 0; cur != end; ++cur, index++) {
        // ignore first texture as that'll be the input of the last pass/image (unless the image is an FBO)
        if (index == 0)
            continue;

        if (cur->second.find ("_rt_") == 0) {
            const CFBO* fbo = this->m_material->m_effect->findFBO (cur->second);

            if (fbo == nullptr)
                fbo = this->m_material->getImage ()->getScene ()->findFBO (cur->second);

            if (fbo != nullptr) {
                this->m_fbos.insert (std::make_pair (index, fbo));
                this->m_textures.emplace_back (fbo);
            }
            // _rt_texture
        } else {
            if (cur->second.empty ()) {
                this->m_textures.emplace_back (nullptr);
            } else {
                this->m_textures.emplace_back (this->m_material->getImage ()->getContext ().resolveTexture (cur->second));
            }
        }
    }
}

void CPass::setupShaderVariables () {
    // find variables in the shaders and set the value with the constants if possible
    for (const auto& [name, value] : this->m_pass->getConstants ()) {
        const auto parameters = this->m_shader->findParameter (name);

        // variable not found, can be ignored
        if (parameters.vertex == nullptr && parameters.fragment == nullptr)
            continue;

        // get one instance of it
        CShaderVariable* var = parameters.vertex == nullptr ? parameters.fragment : parameters.vertex;

        // ensure the shader's and the constant are of the same type
        if (value->getType () == var->getType ()) {
            this->addUniform (var->getName (), value);
            continue;
        }

        // there's situations where this type mismatch is actually expected
        // integers and floats are equivalent, this could be detected at load time
        // but that'd mean to compile the shader in the load, and not on the render stage
        // so take into account these conversions here
        if (value->is<CShaderConstantFloat> () && var->is<CShaderVariableInteger> ()) {
            // create an integer value from a float
            this->addUniform (var->getName (), static_cast<int> (*value->as<CShaderConstantFloat> ()->getValue ()));
        } else if (value->is<CShaderConstantInteger> () && var->is<CShaderVariableFloat> ()) {
            // create a float value from an integer
            this->addUniform (var->getName (), static_cast<float> (*value->as<CShaderConstantInteger> ()->getValue ()));
        } else if (value->is<CShaderConstantVector4> () && var->is<CShaderVariableVector2> ()) {
            auto* val = value->as<CShaderConstantVector4> ();

            // create a new vector2 with the first two values
            this->addUniform (var->getName (), {val->getValue ()->x, val->getValue ()->y});
        } else if (value->is<CShaderConstantVector4> () && var->is<CShaderVariableVector3> ()) {
            auto* val = value->as<CShaderConstantVector4> ();

            this->addUniform (var->getName (), {val->getValue ()->x, val->getValue ()->y, val->getValue ()->z});
        } else {
            sLog.exception ("Constant ", name,
                            " type does not match pixel/vertex shader variable and cannot be converted (",
                            value->getType (), " to ", var->getType ());
        }
    }

    for (const auto& cur : this->m_shader->getVertexParameters ())
        if (this->m_uniforms.find (cur->getName ()) == this->m_uniforms.end ())
            this->addUniform (cur);

    for (const auto& cur : this->m_shader->getFragmentParameters ())
        if (this->m_uniforms.find (cur->getName ()) == this->m_uniforms.end ())
            this->addUniform (cur);
}

// define some basic methods for the template
void CPass::addUniform (CShaderVariable* value) {
    if (value->is<CShaderVariableFloat> ())
        this->addUniform (value->getName (),
                          static_cast<const float*> (value->as<CShaderVariableFloat> ()->getValue ()));
    else if (value->is<CShaderVariableInteger> ())
        this->addUniform (value->getName (),
                          static_cast<const int*> (value->as<CShaderVariableInteger> ()->getValue ()));
    else if (value->is<CShaderVariableVector2> ())
        this->addUniform (value->getName (),
                          static_cast<const glm::vec2*> (value->as<CShaderVariableVector2> ()->getValue ()));
    else if (value->is<CShaderVariableVector3> ())
        this->addUniform (value->getName (),
                          static_cast<const glm::vec3*> (value->as<CShaderVariableVector3> ()->getValue ()));
    else if (value->is<CShaderVariableVector4> ())
        this->addUniform (value->getName (),
                          static_cast<const glm::vec4*> (value->as<CShaderVariableVector4> ()->getValue ()));
}

void CPass::addUniform (const std::string& name, CShaderConstant* value) {
    // now determine the constant's type and register the correct uniform for it
    if (value->is<CShaderConstantFloat> ())
        this->addUniform (name, value->as<CShaderConstantFloat> ()->getValue ());
    else if (value->is<CShaderConstantInteger> ())
        this->addUniform (name, value->as<CShaderConstantInteger> ()->getValue ());
    else if (value->is<CShaderConstantVector4> ())
        this->addUniform (name, value->as<CShaderConstantVector4> ()->getValue ());
}

void CPass::addUniform (const std::string& name, const CShaderConstant* value) {
    // now determine the constant's type and register the correct uniform for it
    if (value->is<CShaderConstantFloat> ())
        this->addUniform (name, value->as<CShaderConstantFloat> ()->getValue ());
    else if (value->is<CShaderConstantInteger> ())
        this->addUniform (name, value->as<CShaderConstantInteger> ()->getValue ());
    else if (value->is<CShaderConstantVector4> ())
        this->addUniform (name, value->as<CShaderConstantVector4> ()->getValue ());
}
void CPass::addUniform (const std::string& name, int value) {
    this->addUniform (name, UniformType::Integer, value);
}

void CPass::addUniform (const std::string& name, const int* value, int count) {
    this->addUniform (name, UniformType::Integer, value, count);
}

void CPass::addUniform (const std::string& name, const int** value) {
    this->addUniform (name, UniformType::Integer, value);
}

void CPass::addUniform (const std::string& name, double value) {
    this->addUniform (name, UniformType::Double, value);
}

void CPass::addUniform (const std::string& name, const double* value, int count) {
    this->addUniform (name, UniformType::Double, value, count);
}

void CPass::addUniform (const std::string& name, const double** value) {
    this->addUniform (name, UniformType::Double, value);
}

void CPass::addUniform (const std::string& name, float value) {
    this->addUniform (name, UniformType::Float, value);
}

void CPass::addUniform (const std::string& name, const float* value, int count) {
    this->addUniform (name, UniformType::Float, value, count);
}

void CPass::addUniform (const std::string& name, const float** value) {
    this->addUniform (name, UniformType::Float, value);
}

void CPass::addUniform (const std::string& name, glm::vec2 value) {
    this->addUniform (name, UniformType::Vector2, value);
}

void CPass::addUniform (const std::string& name, const glm::vec2* value) {
    this->addUniform (name, UniformType::Vector2, value, 1);
}

void CPass::addUniform (const std::string& name, const glm::vec2** value) {
    this->addUniform (name, UniformType::Vector2, value, 1);
}

void CPass::addUniform (const std::string& name, glm::vec3 value) {
    this->addUniform (name, UniformType::Vector3, value);
}

void CPass::addUniform (const std::string& name, const glm::vec3* value) {
    this->addUniform (name, UniformType::Vector3, value, 1);
}

void CPass::addUniform (const std::string& name, const glm::vec3** value) {
    this->addUniform (name, UniformType::Vector3, value);
}

void CPass::addUniform (const std::string& name, const glm::vec4 value) {
    this->addUniform (name, UniformType::Vector4, value);
}

void CPass::addUniform (const std::string& name, const glm::vec4* value) {
    this->addUniform (name, UniformType::Vector4, value, 1);
}

void CPass::addUniform (const std::string& name, const glm::vec4** value) {
    this->addUniform (name, UniformType::Vector4, value);
}

void CPass::addUniform (const std::string& name, const glm::mat3& value) {
    this->addUniform (name, UniformType::Matrix3, value);
}

void CPass::addUniform (const std::string& name, const glm::mat3* value) {
    this->addUniform (name, UniformType::Matrix3, value, 1);
}

void CPass::addUniform (const std::string& name, const glm::mat3** value) {
    this->addUniform (name, UniformType::Matrix3, value);
}

void CPass::addUniform (const std::string& name, const glm::mat4 value) {
    this->addUniform (name, UniformType::Matrix4, value);
}

void CPass::addUniform (const std::string& name, const glm::mat4* value) {
    this->addUniform (name, UniformType::Matrix4, value, 1);
}

void CPass::addUniform (const std::string& name, const glm::mat4** value) {
    this->addUniform (name, UniformType::Matrix4, value);
}
