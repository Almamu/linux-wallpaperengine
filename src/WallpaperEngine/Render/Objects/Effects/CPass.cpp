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

extern double g_Time;

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
    if (drawTo == 0)
        std::cout << "FINAL PASS TO SCREEN --------------------------\n";
    else
        std::cout << "NEW PASS --------------------------------------\n";

    // clear whatever buffer we're drawing to if we're not drawing to screen
    if (drawTo > 0)
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // use the shader we have registered
    glUseProgram (this->m_programID);

    // bind the input texture
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, input);
    std::cout << "Binding texture " << 0 << " to " << input << "\n";

    // first bind the textures to their sampler place
    {
        // set texture slots for the shader
        auto cur = this->m_textures.begin ();
        auto end = this->m_textures.end ();

        for (int index = 1; cur != end; cur ++, index ++)
        {
            std::cout << "Binding texture " << index << " to " << (*cur)->getTextureID () << "\n";

            // set the active texture index
            glActiveTexture (GL_TEXTURE0 + index);
            // bind the correct texture there
            glBindTexture (GL_TEXTURE_2D, (*cur)->getTextureID ());
        }
    }

    // now bind variables needed
    {
        auto cur = this->m_variables.begin ();
        auto end = this->m_variables.end ();

        for (; cur != end; cur ++)
        {
#ifdef DEBUG
            std::cout << "(" << (*cur).second->getIdentifierName() << ") - " << (*cur).second->getName () << " = ";
#endif /* DEBUG */

            if ((*cur).second->is <CShaderVariableInteger> () == true)
            {
                GLint value = *static_cast <const int32_t*> ((*cur).second->getValue ());
#ifdef DEBUG
                std::cout << value << "\n";
#endif /* DEBUG */
                glUniform1i ((*cur).first, value);
            }
            else if ((*cur).second->is <CShaderVariableFloat> () == true)
            {
                GLfloat value = *static_cast <const float*> ((*cur).second->getValue ());
#ifdef DEBUG
                std::cout << value << "\n";
#endif /* DEBUG */
                glUniform1f ((*cur).first, value);
            }
            else if ((*cur).second->is <CShaderVariableVector2> () == true)
            {
                glm::vec2 value = *static_cast <const glm::vec2*> ((*cur).second-> as <CShaderVariableVector2> ()->getValue ());
#ifdef DEBUG
                std::cout << value.x << " " << value.y << "\n";
#endif /* DEBUG */
                glUniform2fv ((*cur).first, 1, glm::value_ptr (value));
            }
            else if ((*cur).second->is <CShaderVariableVector3> () == true)
            {
                glm::vec3 value = *static_cast <const glm::vec3*> ((*cur).second-> as <CShaderVariableVector3> ()->getValue ());
#ifdef DEBUG
                std::cout << value.x << " " << value.y << " " << value.z << "\n";
#endif /* DEBUG */
                glUniform3fv ((*cur).first, 1, glm::value_ptr (value));
            }
            else if ((*cur).second->is <CShaderVariableVector4> () == true)
            {
                glm::vec4 value = *static_cast <const glm::vec4*> ((*cur).second-> as <CShaderVariableVector4> ()->getValue ());
#ifdef DEBUG
                std::cout << value.x << " " << value.y << " " << value.z << " " << value.w << "\n";
#endif /* DEBUG */
                glUniform4fv ((*cur).first, 1, glm::value_ptr (value));
            }
            else
            {
#ifdef DEBUG
                std::cout << "null\n";
#endif /* DEBUG */
            }
        }
    }

    // add static things
    if (this->g_Texture0 != -1)
    {
#ifdef DEBUG
        std::cout << "g_Texture0 = 0\n";
#endif /* DEBUG */
        glUniform1i (this->g_Texture0, 0);
    }
    if (this->g_Texture1 != -1)
    {
#ifdef DEBUG
        std::cout << "g_Texture1 = 1\n";
#endif /* DEBUG */
        glUniform1i (this->g_Texture1, 1);
    }
    if (this->g_Texture2 != -1)
    {
#ifdef DEBUG
        std::cout << "g_Texture2 = 2\n";
#endif /* DEBUG */
        glUniform1i (this->g_Texture2, 2);
    }
    if (this->g_Time != -1)
    {
#ifdef DEBUG
        std::cout << "g_Time = " << (float) ::g_Time << "\n";
#endif /* DEBUG */
        glUniform1d (this->g_Time, (float) ::g_Time);
    }
    if (this->g_ModelViewProjectionMatrix != -1)
    {
        // calculate the new matrix
        glm::mat4 projection = this->m_material->getImage ()->getScene ()->getCamera ()->getProjection ();
        glm::mat4 view = this->m_material->getImage ()->getScene ()->getCamera ()->getLookAt ();
        glm::mat4 model = glm::mat4 (1.0f);
        glm::mat4 mvp = projection * view * model;

        glUniformMatrix4fv (this->g_ModelViewProjectionMatrix, 1, GL_FALSE, &mvp [0] [0]);

#ifdef DEBUG
        std::cout << "g_ModelViewProjectionMatrix\n";
#endif /* DEBUG */
    }
    if (this->g_Brightness != -1)
    {
#ifdef DEBUG
        std::cout << "g_Brightness = 1.0\n";
#endif /* DEBUG */
        glUniform1f (this->g_Brightness, 1.0f);
    }
    if (this->g_UserAlpha != -1)
    {
#ifdef DEBUG
        std::cout << "g_UserAlpha = 1.0\n";
#endif /* DEBUG */
        glUniform1f (this->g_UserAlpha, 1.0f);
    }
    if (this->g_Texture0Rotation != -1)
    {
#ifdef DEBUG
        std::cout << "g_Texture0Rotation = {0.0, 0.0}\n";
#endif /* DEBUG */
        glUniform2f (this->g_Texture0Rotation, 0.0f, 0.0f);
    }
    if (this->g_Texture0Translation != -1)
    {
#ifdef DEBUG
        std::cout << "g_Texture0Translation = {0.0, 0.0, 0.0, 0.0}\n";
#endif /* DEBUG */
        glUniform4f (this->g_Texture0Translation, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (this->a_TexCoord != -1)
    {
        std::cout << "a_TexCoord present!\n";
        glEnableVertexAttribArray (this->a_TexCoord);
        glBindBuffer (GL_ARRAY_BUFFER, *this->m_material->getImage ()->getTexCoordBuffer ());
        glVertexAttribPointer (
            this->a_TexCoord,
            2,
            GL_FLOAT,
            GL_FALSE,
            0,
            nullptr
        );
    }
    if (this->a_Position != -1)
    {
        std::cout << "a_Position present!\n";
        glEnableVertexAttribArray (this->a_Position);
        glBindBuffer (GL_ARRAY_BUFFER, (drawTo > 0) ? *this->m_material->getImage ()->getPassVertexBuffer () : *this->m_material->getImage ()->getVertexBuffer ());
        glVertexAttribPointer (
            this->a_Position,
            3,
            GL_FLOAT,
            GL_FALSE,
            0,
            nullptr
        );
    }

    // start actual rendering now
    glBindBuffer (GL_ARRAY_BUFFER, (drawTo > 0) ? *this->m_material->getImage ()->getPassVertexBuffer () : *this->m_material->getImage ()->getVertexBuffer ());
    glDrawArrays (GL_TRIANGLES, 0, 6);

    if (this->a_Position != -1)
        glDisableVertexAttribArray (this->a_Position);
    if (this->a_TexCoord != -1)
        glDisableVertexAttribArray (this->a_TexCoord);
}

/*
void CPass::OnSetConstants (irr::video::IMaterialRendererServices *services, int32_t userData)
{
    // TODO: REWRITE
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
                (float*) (*cur)->getValue (),
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
                (float*) (*cur)->getValue (),
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
                (float*) (*cur)->getValue (),
                (*cur)->getSize ()
            );
            services->setVertexShaderConstant (
                (*cur)->getName ().c_str (),
                (float*) (*cur)->getValue (),
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

    float textureRotation [4] = { image->getAngles ().X, image->getAngles ().Y, image->getAngles ().Z, image->getAngles ().Z };

    for (int index = 0; textureCur != textureEnd; textureCur ++, index ++)
    {
        // TODO: CHECK THESE VALUES, DOCUMENTATION SAYS THAT FIRST TWO ELEMENTS SHOULD BE WIDTH AND HEIGHT
        // TODO: BUT IN REALITY THEY DO NOT SEEM TO BE THAT, NOT HAVING SUPPORT FOR ATTRIBUTES DOESN'T HELP EITHER
        float textureResolution [4] = {
            1.0, -1.0, 1.0, 1.0
        };
        float textureTranslation [2] = {
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
*/

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

    // get information from the program, like uniforms, etc
    // support three textures for now
    this->g_Texture0 = glGetUniformLocation (this->m_programID, "g_Texture0");
    this->g_Texture1 = glGetUniformLocation (this->m_programID, "g_Texture1");
    this->g_Texture2 = glGetUniformLocation (this->m_programID, "g_Texture2");
    this->g_Time = glGetUniformLocation (this->m_programID, "g_Time");
    this->g_Texture0Rotation = glGetUniformLocation (this->m_programID, "g_Texture0Rotation");
    this->g_Texture0Translation = glGetUniformLocation (this->m_programID, "g_Texture0Translation");
    this->g_ModelViewProjectionMatrix = glGetUniformLocation (this->m_programID, "g_ModelViewProjectionMatrix");
    this->g_UserAlpha = glGetUniformLocation (this->m_programID, "g_UserAlpha");
    this->g_Brightness = glGetUniformLocation (this->m_programID, "g_Brightness");

    // bind a_TexCoord and a_Position
    this->a_TexCoord = glGetAttribLocation (this->m_programID, "a_TexCoord");
    this->a_Position = glGetAttribLocation (this->m_programID, "a_Position");
}

void CPass::setupTextures ()
{
    auto cur = this->m_pass->getTextures ().begin ();
    auto end = this->m_pass->getTextures ().end ();

    for (; cur != end; cur ++)
    {
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

            // get the uniform first
            GLint uniform = glGetUniformLocation (this->m_programID, pixelVar->getName ().c_str ());

            if (uniform != -1)
                // register the variable
                this->m_variables.insert (std::make_pair (uniform, pixelVar));
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

            // get the uniform first
            GLint uniform = glGetUniformLocation (this->m_programID, vertexVar->getName ().c_str ());

            if (uniform != -1)
                // register the variable
                this->m_variables.insert (std::make_pair (uniform, vertexVar));
        }
    }
}
