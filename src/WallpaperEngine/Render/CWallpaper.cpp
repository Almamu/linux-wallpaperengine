#include "CWallpaper.h"
#include "CScene.h"
#include "CVideo.h"

#include <utility>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace WallpaperEngine::Render;

CWallpaper::CWallpaper (Core::CWallpaper* wallpaperData, std::string type, CContainer* container, CContext* context) :
    m_container (container),
    m_wallpaperData (wallpaperData),
    m_type (std::move(type)),
    m_context (context)
{
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

CContainer* CWallpaper::getContainer () const
{
    return this->m_container;
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
    return this->m_sceneFBO->getTextureID();
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
        throw std::runtime_error (message);
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
        throw std::runtime_error (message);
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
        throw std::runtime_error (message);
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

void CWallpaper::render (glm::vec4 viewport, bool drawToBackground, char* image_data)
{
    this->renderFrame (viewport);

    int windowWidth = 1920;
    int windowHeight = 1080;

    if (this->getWallpaperData ()->is <WallpaperEngine::Core::CScene> ())
    {
        auto projection = this->getWallpaperData ()->as <WallpaperEngine::Core::CScene> ()->getOrthogonalProjection ();

        windowWidth = projection->getWidth ();
        windowHeight = projection->getHeight ();
    }
    else if (this->is <WallpaperEngine::Render::CVideo> ())
    {
        auto video = this->as <WallpaperEngine::Render::CVideo> ();

        windowWidth = video->getWidth ();
        windowHeight = video->getHeight ();
    }

    float widthRatio = windowWidth / viewport.z;
    float heightRatio = windowHeight / viewport.w;

    if (widthRatio > 1.0f)
    {
        float diff = widthRatio - 1.0f;

        widthRatio -= diff;
        heightRatio -= diff;
    }

    if (heightRatio > 1.0f)
    {
        float diff = heightRatio - 1.0f;

        widthRatio -= diff;
        heightRatio -= diff;
    }

    if (widthRatio < 1.0f)
    {
        float diff = 1.0f - widthRatio;

        widthRatio += diff;
        heightRatio += diff;
    }

    if (heightRatio < 1.0f)
    {
        float diff = 1.0f - heightRatio;

        widthRatio += diff;
        heightRatio += diff;
    }

    if (widthRatio < 0.0f) widthRatio = -widthRatio;
    if (heightRatio < 0.0f) heightRatio = -heightRatio;

    GLfloat position [] = {
        -widthRatio, -heightRatio, 0.0f,
        widthRatio, -heightRatio, 0.0f,
        -widthRatio, heightRatio, 0.0f,
        -widthRatio, heightRatio, 0.0f,
        widthRatio, -heightRatio, 0.0f,
        widthRatio, heightRatio, 0.0f
    };

    glBindBuffer (GL_ARRAY_BUFFER, this->m_positionBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (position), position, GL_STATIC_DRAW);

    // we only want texCoords to be set once
    static bool setTexCoords = true;
    if (setTexCoords)
    {
        setTexCoords = false;
        if (drawToBackground)
        {
            // Need to flip the image (FB stores the image upside down)
            GLfloat texCoords [] = {
                0.0f, 1.0f,
                1.0f, 1.0f,
                0.0f, 0.0f,
                0.0f, 0.0f,
                1.0f, 1.0f,
                1.0f, 0.0f
            };
            glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
            glBufferData (GL_ARRAY_BUFFER, sizeof (texCoords), texCoords, GL_STATIC_DRAW);
        }
        else 
        {
            GLfloat texCoords [] = {
                0.0f, 0.0f,
                1.0f, 0.0f,
                0.0f, 1.0f,
                0.0f, 1.0f,
                1.0f, 0.0f,
                1.0f, 1.0f
            };
            glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
            glBufferData (GL_ARRAY_BUFFER, sizeof (texCoords), texCoords, GL_STATIC_DRAW);
        }
    }

    glViewport (viewport.x, viewport.y, viewport.z, viewport.w);

    static CFBO* screen_fbo = new CFBO("_sc_FullFrameBuffer", ITexture::TextureFormat::ARGB8888, 1.0, windowWidth, windowHeight, windowWidth, windowHeight);
    
    if (drawToBackground)
        // write to screen buffer
        glBindFramebuffer (GL_FRAMEBUFFER, screen_fbo->getFramebuffer());
    else
        // write to default's framebuffer
        glBindFramebuffer (GL_FRAMEBUFFER, GL_NONE);

    
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
    glVertexAttribPointer (this->a_TexCoord, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glEnableVertexAttribArray (this->a_Position);
    glBindBuffer (GL_ARRAY_BUFFER, this->m_positionBuffer);
    glVertexAttribPointer (this->a_Position, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glUniform1i (this->g_Texture0, 0);
    // write the framebuffer as is to the screen
    glBindBuffer (GL_ARRAY_BUFFER, this->m_texCoordBuffer);
    glDrawArrays (GL_TRIANGLES, 0, 6);

    // Get FB data from OpenGL, X11 will free this pointer when it is created into an XImage.
    if (image_data)
        glReadPixels(0, 0, 1920, 1080, GL_BGRA,  GL_UNSIGNED_BYTE, (void*)(image_data));
}

void CWallpaper::setupFramebuffers ()
{
    int windowWidth = 1920;
    int windowHeight = 1080;

    if (this->getWallpaperData ()->is <WallpaperEngine::Core::CScene> ())
    {
        auto projection = this->getWallpaperData ()->as <WallpaperEngine::Core::CScene> ()->getOrthogonalProjection ();

        windowWidth = projection->getWidth ();
        windowHeight = projection->getHeight ();
    }
    else if (this->is <WallpaperEngine::Render::CVideo> ())
    {
        auto video = this->as <WallpaperEngine::Render::CVideo> ();

        windowWidth = video->getWidth ();
        windowHeight = video->getHeight ();
    }

    // create framebuffer for the scene
    this->m_sceneFBO = this->createFBO ("_rt_FullFrameBuffer", ITexture::TextureFormat::ARGB8888, 1.0, windowWidth, windowHeight, windowWidth, windowHeight);
}

CContext* CWallpaper::getContext ()
{
    return this->m_context;
}

CFBO* CWallpaper::createFBO (const std::string& name, ITexture::TextureFormat format, float scale, uint32_t realWidth, uint32_t realHeight, uint32_t textureWidth, uint32_t textureHeight)
{
    CFBO* fbo = new CFBO (name, format, scale, realWidth, realHeight, textureWidth, textureHeight);

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
        throw std::runtime_error ("Cannot find given FBO");

    return it->second;
}

CFBO* CWallpaper::getFBO () const
{
    return this->m_sceneFBO;
}