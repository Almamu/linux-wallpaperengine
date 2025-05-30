#include "CPass.h"
#include "WallpaperEngine/Render/CFBO.h"
#include <sstream>
#include <utility>

#include "WallpaperEngine/Core/Projects/CProperty.h"
#include "WallpaperEngine/Core/Projects/CPropertyColor.h"
#include "WallpaperEngine/Core/Projects/CPropertyCombo.h"
#include "WallpaperEngine/Core/Projects/CPropertySlider.h"
#include "WallpaperEngine/Core/Projects/CPropertyBoolean.h"

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableFloat.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableInteger.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector2.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector3.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector4.h"

#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector2.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector3.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantProperty.h"
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
    this->setupShaders ();
    this->setupShaderVariables ();
}

std::shared_ptr<const ITexture> CPass::resolveTexture (std::shared_ptr<const ITexture> expected, int index, std::shared_ptr<const ITexture> previous) {
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
    return this->resolveFBO (it->second->getName ());
}

std::shared_ptr<const CFBO> CPass::resolveFBO (const std::string& name) const {
    auto fbo = this->m_material->getEffect()->findFBO (name);

    if (fbo == nullptr) {
        fbo = this->m_material->getImage ()->getScene ()->findFBO (name);
    }

    if (fbo == nullptr) {
        sLog.exception ("Tried to resolve and FBO without any luck: ", name);
    }

    return fbo;
}

void CPass::setupRenderFramebuffer () {
    // set the framebuffer we're drawing to
    glBindFramebuffer (GL_FRAMEBUFFER, this->m_drawTo->getFramebuffer ());

    // set proper viewport based on what we're drawing to
    glViewport (0, 0, this->m_drawTo->getRealWidth (), this->m_drawTo->getRealHeight ());

    // set texture blending
    if (this->getBlendingMode () == "translucent") {
        glEnable (GL_BLEND);
        glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else if (this->getBlendingMode () == "additive") {
        glEnable (GL_BLEND);
        glBlendFuncSeparate (GL_SRC_ALPHA, GL_ONE, GL_SRC_ALPHA, GL_ONE);
    } else if (this->getBlendingMode () == "normal") {
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
    std::shared_ptr<const ITexture> texture = this->resolveTexture (this->m_input, 0, this->m_input);

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
    if (!this->m_textures.empty ()) {
        for (const auto& [index, expectedTexture] : this->m_textures) {
            if (expectedTexture == nullptr) {
                texture = this->m_input;
            } else {
                texture = expectedTexture;
            }

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
    if (!this->m_textures.empty ()) {
        for (const auto& [index, _] : this->m_textures) {
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

void CPass::setDestination (std::shared_ptr<const CFBO> drawTo) {
    this->m_drawTo = std::move(drawTo);
}

void CPass::setInput (std::shared_ptr<const ITexture> input) {
    this->m_input = std::move(input);
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
    this->m_blendingmode = std::move(blendingmode);
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

        if (result == GL_FALSE) {
            // shader compilation failed completely, throw an exception
            sLog.exception (buffer.str ());
        } else {
            // some warning was emitted, log the error and keep chuging along
            sLog.error (buffer.str ());
        }
    }

    return shaderID;
}

void CPass::setupShaders () {
    // ensure the constants are defined
    std::shared_ptr<const ITexture> texture0 = this->m_material->getImage ()->getTexture ();

    // copy the combos from the pass
    this->m_combos.insert (this->m_pass->getCombos ().begin (), this->m_pass->getCombos ().end ());

    // TODO: THE VALUES ARE THE SAME AS THE ENUMERATION, SO MAYBE IT HAS TO BE SPECIFIED FOR THE TEXTURE 0 OF ALL
    // ELEMENTS?
    if (texture0 != nullptr) {
        if (texture0->getFormat () == ITexture::TextureFormat::RG88) {
            this->m_combos.insert_or_assign ("TEX0FORMAT", 8);
        } else if (texture0->getFormat () == ITexture::TextureFormat::R8) {
            this->m_combos.insert_or_assign ("TEX0FORMAT", 9);
        }
    }

    // TODO: REVIEW THE SHADER TEXTURES HERE, THE ONES PASSED ON TO THE SHADER SHOULD NOT BE IN THE LIST
    // TODO: USED TO BUILD THE TEXTURES LATER
    // use the combos copied from the pass so it includes the texture format
    this->m_shader = new Render::Shaders::CShader (
        this->getMaterial ()->getImage ()->getContainer (), this->m_pass->getShader (), this->m_combos,
        this->m_pass->getTextures (), this->m_pass->getConstants ()
    );

    const auto shaders = Shaders::CGLSLContext::get ().toGlsl (
        this->m_shader->vertex (), this->m_shader->fragment ());

    // compile the shaders
    const GLuint vertexShaderID = compileShader (shaders.first.c_str (), GL_VERTEX_SHADER);
    const GLuint fragmentShaderID = compileShader (shaders.second.c_str (), GL_FRAGMENT_SHADER);
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
        if (result == GL_FALSE) {
            // shader compilation failed completely, throw an exception
            sLog.exception (message);
        } else {
            // some warning was emitted, log the error and keep chuging along
            sLog.error (message);
        }
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
    // first set default textures extracted from the shader
    // vertex shader doesn't seem to have texture info
    // but for now just set first vertex's textures
    // and then try with fragment's and override any existing
    for (const auto& [index, textureName] : this->m_shader->getVertex ().getTextures ()) {
        try {
            // resolve the texture first
            std::shared_ptr<const ITexture> textureRef;

            if (textureName.find ("_rt_") == 0 || textureName.find ("_alias_") == 0) {
                textureRef = this->resolveFBO (textureName);
            } else {
                textureRef = this->getContext ().resolveTexture (textureName);
            }

            this->m_textures [index] = textureRef;
        } catch (std::runtime_error& ex) {
            sLog.error ("Cannot resolve texture ", textureName, " for fragment shader ", ex.what ());
        }
    }

    for (const auto& [index, textureName] : this->m_shader->getFragment ().getTextures ()) {
        try {
            // resolve the texture first
            std::shared_ptr<const ITexture> textureRef;

            if (textureName.find ("_rt_") == 0 || textureName.find ("_alias_") == 0) {
                textureRef = this->resolveFBO (textureName);
            } else {
                textureRef = this->getContext ().resolveTexture (textureName);
            }

            this->m_textures [index] = textureRef;
        } catch (std::runtime_error& ex) {
            sLog.error ("Cannot resolve texture ", textureName, " for fragment shader ", ex.what ());
        }
    }

    for (const auto& [index, textureName] : this->m_pass->getTextures ()) {
        // ignore first texture as that'll be the input of the previous pass (or the image if it's the first pass)
        if (index == 0) {
            continue;
        }

        if (textureName.find ("_rt_") == 0) {
            this->m_textures[index] = this->resolveFBO (textureName);
        } else if (!textureName.empty ()) {
            this->m_textures[index] = this->m_material->getImage ()->getContext ().resolveTexture (textureName);
        }
    }

    // binds are set last as they're the most important to be set
    for (const auto& [index, bind] : this->m_material->getMaterial ()->getTextureBinds ()) {
        if (bind->getName () == "previous") {
            // use nullptr as indication for "previous" texture
            this->m_textures [index] = nullptr;
        } else {
            // a normal bind, search for the corresponding FBO and set it
            this->m_textures [index] = this->resolveFBO (bind->getName ());
        }
    }

    // resolve the main texture
    std::shared_ptr<const ITexture> texture = this->resolveTexture (this->m_material->getImage ()->getTexture (), 0);
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

    for (const auto& [textureIndex, expectedTexture] : this->m_textures) {
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

    // free the uniform that's already registered if it's there already
    const auto it = this->m_uniforms.find (name);

    if (it != this->m_uniforms.end ()) {
        delete it->second;
    }

    // build a copy of the value and allocate it somewhere
    T* newValue = new T (value);

    // uniform found, add it to the list
    this->m_uniforms.insert_or_assign (name, new UniformEntry (id, name, type, newValue, 1));
}

template <typename T> void CPass::addUniform (const std::string& name, UniformType type, T* value, int count) {
    // this version is used to reference to system variables so things like g_Time works fine
    GLint id = glGetUniformLocation (this->m_programID, name.c_str ());

    // parameter not found, can be ignored
    if (id == -1)
        return;

    // free the uniform that's already registered if it's there already
    const auto it = this->m_uniforms.find (name);

    if (it != this->m_uniforms.end ()) {
        delete it->second;
    }

    // uniform found, add it to the list
    this->m_uniforms.insert_or_assign (name, new UniformEntry (id, name, type, value, count));
}

template <typename T> void CPass::addUniform (const std::string& name, UniformType type, T** value) {
    // this version is used to reference to system variables so things like g_Time works fine
    const GLint id = glGetUniformLocation (this->m_programID, name.c_str ());

    // parameter not found, can be ignored
    if (id == -1)
        return;

    // free the uniform that's already registered if it's there already
    const auto it = this->m_uniforms.find (name);

    if (it != this->m_uniforms.end ()) {
        delete it->second;
    }

    // uniform found, add it to the list
    this->m_referenceUniforms.insert_or_assign (
        name, new ReferenceUniformEntry (id, name, type, reinterpret_cast<const void**> (value)));
}

void CPass::setupShaderVariables () {
    for (const auto& cur : this->m_shader->getVertex ().getParameters ())
        if (this->m_uniforms.find (cur->getName ()) == this->m_uniforms.end ())
            this->addUniform (cur);

    for (const auto& cur : this->m_shader->getFragment ().getParameters ())
        if (this->m_uniforms.find (cur->getName ()) == this->m_uniforms.end ())
            this->addUniform (cur);

    // find variables in the shaders and set the value with the constants if possible
    for (const auto& [name, value] : this->m_pass->getConstants ()) {
        const auto parameters = this->m_shader->findParameter (name);

        // variable not found, can be ignored
        if (parameters.vertex == nullptr && parameters.fragment == nullptr)
            continue;

        // get one instance of it
        CShaderVariable* var = parameters.vertex == nullptr ? parameters.fragment : parameters.vertex;

        // this takes care of all possible casts, even invalid ones, which will use whatever default behaviour
        // of the underlying CDynamicValue used for the value
        this->addUniform (var, value);
    }
}

// define some basic methods for the template
void CPass::addUniform (CShaderVariable* value) {
    // no need to re-implement this, call the version that takes a CDynamicValue as second parameter
    // and that handles casting and everything
    this->addUniform (value, value);
}

void CPass::addUniform (CShaderVariable* value, const CDynamicValue* setting) {
    if (value->is<CShaderVariableFloat> ()) {
        this->addUniform (value->getName (), setting->getFloat ());
    } else if (value->is<CShaderVariableInteger> ()) {
        this->addUniform (value->getName (), setting->getInt ());
    } else if (value->is<CShaderVariableVector2> ()) {
        this->addUniform (value->getName (), setting->getVec2 ());
    } else if (value->is<CShaderVariableVector3> ()) {
        this->addUniform (value->getName (), setting->getVec3 ());
    } else if (value->is<CShaderVariableVector4> ()) {
        this->addUniform (value->getName (), setting->getVec4 ());
    } else {
        sLog.error ("Cannot convert setting dynamic value  to ", value->getName (), ". Using default value");
    }
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
