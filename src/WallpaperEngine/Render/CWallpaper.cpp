#include "common.h"
#include "CWallpaper.h"
#include "CScene.h"
#include "CVideo.h"

#include <utility>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace WallpaperEngine::Render;

CWallpaper::CWallpaper (Core::CWallpaper* wallpaperData, std::string type, CRenderContext* context) :
    m_wallpaperData (wallpaperData),
    m_type (std::move(type)),
    m_context (context),
    m_destFramebuffer (GL_NONE)
{
    // generate the VAO to stop opengl from complaining
    glGenVertexArrays (1, &this->m_vaoBuffer);
    glBindVertexArray (this->m_vaoBuffer);

    this->setupShaders ();

    GLfloat texCoords [] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f
    };

    // inverted positions so the final texture is rendered properly
    GLfloat position [] = {
        -1.0f, 1.0f, 0.0f,
        1.0, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f
    };

    glGenBuffers (1, &this->m_texCoordBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (texCoords), texCoords, GL_STATIC_DRAW);

    glGenBuffers (1, &this->m_positionBuffer);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_positionBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (position), position, GL_STATIC_DRAW);
}

CWallpaper::~CWallpaper ()
{
}

const CContainer* CWallpaper::getContainer () const
{
    return this->m_context->getContainer ();
}

WallpaperEngine::Core::CWallpaper* CWallpaper::getWallpaperData ()
{
    return this->m_wallpaperData;
}

GLuint CWallpaper::getWallpaperFramebuffer () const
{
    return this->m_sceneFBO->getFramebuffer ();
}

GLuint CWallpaper::getWallpaperTexture () const
{
    return this->m_sceneFBO->getTextureID(0);
}

void CWallpaper::setupShaders ()
{
    // reserve shaders in OpenGL
    GLuint vertexShaderID = glCreateShader (GL_VERTEX_SHADER);

    // give shader's source code to OpenGL to be compiled
    const char* sourcePointer = "#version 120\n"
                                "attribute vec3 a_Position;\n"
                                "attribute vec2 a_TexCoord;\n"
                                "varying vec2 v_TexCoord;\n"
                                "void main () {\n"
                                "gl_Position = vec4 (a_Position, 1.0);\n"
                                "v_TexCoord = a_TexCoord;\n"
                                "}";

    glShaderSource (vertexShaderID, 1, &sourcePointer, nullptr);
    glCompileShader (vertexShaderID);

    GLint result = GL_FALSE;
    int infoLogLength = 0;

    // ensure the vertex shader was correctly compiled
    glGetShaderiv (vertexShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv (vertexShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0)
    {
        char* logBuffer = new char [infoLogLength + 1];
        // ensure logBuffer ends with a \0
        memset (logBuffer, 0, infoLogLength + 1);
        // get information about the error
        glGetShaderInfoLog (vertexShaderID, infoLogLength, nullptr, logBuffer);
        // throw an exception about the issue
        std::string message = logBuffer;
        // free the buffer
        delete[] logBuffer;
        // throw an exception
        sLog.exception (message);
    }

    // reserve shaders in OpenGL
    GLuint fragmentShaderID = glCreateShader (GL_FRAGMENT_SHADER);

    // give shader's source code to OpenGL to be compiled
    sourcePointer = "#version 120\n"
                    "uniform sampler2D g_Texture0;\n"
                    "varying vec2 v_TexCoord;\n"
                    "void main () {\n"
                    "gl_FragColor = texture2D (g_Texture0, v_TexCoord);\n"
                    "}";

    glShaderSource (fragmentShaderID, 1, &sourcePointer, nullptr);
    glCompileShader (fragmentShaderID);

    result = GL_FALSE;
    infoLogLength = 0;

    // ensure the vertex shader was correctly compiled
    glGetShaderiv (fragmentShaderID, GL_COMPILE_STATUS, &result);
    glGetShaderiv (fragmentShaderID, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0)
    {
        char* logBuffer = new char [infoLogLength + 1];
        // ensure logBuffer ends with a \0
        memset (logBuffer, 0, infoLogLength + 1);
        // get information about the error
        glGetShaderInfoLog (fragmentShaderID, infoLogLength, nullptr, logBuffer);
        // throw an exception about the issue
        std::string message = logBuffer;
        // free the buffer
        delete[] logBuffer;
        // throw an exception
        sLog.exception (message);
    }

    // create the final program
    this->m_shader = glCreateProgram ();
    // link the shaders together
    glAttachShader (this->m_shader, vertexShaderID);
    glAttachShader (this->m_shader, fragmentShaderID);
    glLinkProgram (this->m_shader);
    // check that the shader was properly linked
    result = GL_FALSE;
    infoLogLength = 0;

    glGetProgramiv (this->m_shader, GL_LINK_STATUS, &result);
    glGetProgramiv (this->m_shader, GL_INFO_LOG_LENGTH, &infoLogLength);

    if (infoLogLength > 0)
    {
        char* logBuffer = new char [infoLogLength + 1];
        // ensure logBuffer ends with a \0
        memset (logBuffer, 0, infoLogLength + 1);
        // get information about the error
        glGetProgramInfoLog (this->m_shader, infoLogLength, nullptr, logBuffer);
        // throw an exception about the issue
        std::string message = logBuffer;
        // free the buffer
        delete[] logBuffer;
        // throw an exception
        sLog.exception (message);
    }

    // after being liked shaders can be dettached and deleted
    glDetachShader (this->m_shader, vertexShaderID);
    glDetachShader (this->m_shader, fragmentShaderID);

    glDeleteShader (vertexShaderID);
    glDeleteShader (fragmentShaderID);

    // get textures
    this->g_Texture0 = glGetUniformLocation (this->m_shader, "g_Texture0");
    this->a_Position = glGetAttribLocation (this->m_shader, "a_Position");
    this->a_TexCoord = glGetAttribLocation (this->m_shader, "a_TexCoord");
}

void CWallpaper::updateTexCoord (GLfloat* texCoords, GLsizeiptr size) const
{
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glBufferData (GL_ARRAY_BUFFER, size, texCoords, GL_STATIC_DRAW);
}
void CWallpaper::setDestinationFramebuffer (GLuint framebuffer)
{
    this->m_destFramebuffer = framebuffer;
}

void CWallpaper::render (glm::ivec4 viewport, bool vflip, bool renderFrame, bool newFrame)
{
    if (renderFrame == true)
        this->renderFrame (viewport);

    int projectionWidth = 1920;
    int projectionHeight = 1080;

    if (this->getWallpaperData ()->is <WallpaperEngine::Core::CScene> ())
    {
        auto projection = this->getWallpaperData ()->as <WallpaperEngine::Core::CScene> ()->getOrthogonalProjection ();

        projectionWidth = projection->getWidth ();
        projectionHeight = projection->getHeight ();
    }
    else if (this->is <WallpaperEngine::Render::CVideo> ())
    {
        auto video = this->as <WallpaperEngine::Render::CVideo> ();

        projectionWidth = video->getWidth ();
        projectionHeight = video->getHeight ();
    }

    float ustart = 0.0f;
    float uend = 0.0f;
    float vstart = 0.0f;
    float vend = 0.0f;

    if (
        (viewport.w > viewport.z && projectionWidth >= projectionHeight) ||
        (viewport.z > viewport.w && projectionHeight > projectionWidth)
        )
    {
        if (vflip)
        {
            vstart = 0.0f;
            vend = 1.0f;
        }
        else
        {
            vstart = 1.0f;
            vend = 0.0f;
        }

        int newWidth = viewport.w / (float) projectionHeight * projectionWidth;
        float newCenter = newWidth / 2.0f;
        float viewportCenter = viewport.z / 2.0;

        float left = newCenter - viewportCenter;
        float right = newCenter + viewportCenter;

        ustart = left / newWidth;
        uend = right / newWidth;
    }

    if (
        (viewport.z > viewport.w && projectionWidth >= projectionHeight) ||
        (viewport.w > viewport.z && projectionHeight > projectionWidth)
        )
    {
        ustart = 0.0f;
        uend = 1.0f;

        int newHeight = viewport.z / (float) projectionWidth * projectionHeight;
        float newCenter = newHeight / 2.0f;
        float viewportCenter = viewport.w / 2.0;

        float down = newCenter - viewportCenter;
        float up = newCenter + viewportCenter;

        if (vflip)
        {
            vstart = down / newHeight;
            vend = up / newHeight;
        }
        else
        {
            vstart = up / newHeight;
            vend = down / newHeight;
        }
    }

    GLfloat texCoords [] = {
        ustart, vstart,
        uend, vstart,
        ustart, vend,
        ustart, vend,
        uend, vstart,
        uend, vend,
    };

    glViewport (viewport.x, viewport.y, viewport.z, viewport.w);

    glBindFramebuffer (GL_FRAMEBUFFER, this->m_destFramebuffer);

    if (newFrame)
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable (GL_BLEND);
    glDisable (GL_DEPTH_TEST);
    // do not use any shader
    glUseProgram (this->m_shader);
    // activate scene texture
    glActiveTexture (GL_TEXTURE0);
    glBindTexture (GL_TEXTURE_2D, this->getWallpaperTexture ());
    // set uniforms and attribs
    glEnableVertexAttribArray (this->a_TexCoord);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (texCoords), texCoords, GL_STATIC_DRAW);
    glVertexAttribPointer (this->a_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glEnableVertexAttribArray (this->a_Position);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_positionBuffer);
    glVertexAttribPointer (this->a_Position, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glUniform1i (this->g_Texture0, 0);
    // write the framebuffer as is to the screen
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glDrawArrays (GL_TRIANGLES, 0, 6);
}

void CWallpaper::setupFramebuffers ()
{
    int projectionWidth = 1920;
    int projectionHeight = 1080;

    if (this->getWallpaperData ()->is <WallpaperEngine::Core::CScene> ())
    {
        auto projection = this->getWallpaperData ()->as <WallpaperEngine::Core::CScene> ()->getOrthogonalProjection ();

        projectionWidth = projection->getWidth ();
        projectionHeight = projection->getHeight ();
    }
    else if (this->is <WallpaperEngine::Render::CVideo> ())
    {
        auto video = this->as <WallpaperEngine::Render::CVideo> ();

        projectionWidth = video->getWidth ();
        projectionHeight = video->getHeight ();
    }

    // create framebuffer for the scene
    this->m_sceneFBO = this->createFBO (
        "_rt_FullFrameBuffer",
        ITexture::TextureFormat::ARGB8888,
        ITexture::TextureFlags::NoInterpolation,
        1.0,
        projectionWidth, projectionHeight,
        projectionWidth, projectionHeight
    );
}

CRenderContext* CWallpaper::getContext ()
{
    return this->m_context;
}

CFBO* CWallpaper::createFBO (const std::string& name, ITexture::TextureFormat format, ITexture::TextureFlags flags, float scale, uint32_t realWidth, uint32_t realHeight, uint32_t textureWidth, uint32_t textureHeight)
{
    CFBO* fbo = new CFBO (name, format, flags, scale, realWidth, realHeight, textureWidth, textureHeight);

    this->m_fbos.insert (std::make_pair (name, fbo));

    return fbo;
}

const std::map<std::string, CFBO*>& CWallpaper::getFBOs () const
{
    return this->m_fbos;
}


CFBO* CWallpaper::findFBO (const std::string& name) const
{
    auto it = this->m_fbos.find (name);

    if (it == this->m_fbos.end ())
        sLog.exception ("Cannot find FBO ", name);

    return it->second;
}

CFBO* CWallpaper::getFBO () const
{
    return this->m_sceneFBO;
}

CWallpaper* CWallpaper::fromWallpaper (Core::CWallpaper* wallpaper, CRenderContext* context)
{
    if (wallpaper->is <Core::CScene> () == true)
        return new WallpaperEngine::Render::CScene (wallpaper->as <Core::CScene> (), context);
    else if (wallpaper->is <Core::CVideo> () == true)
        return new WallpaperEngine::Render::CVideo (wallpaper->as <Core::CVideo> (), context);
    else
        sLog.exception ("Unsupported wallpaper type");
}