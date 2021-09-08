#include "CWallpaper.h"
#include "CScene.h"

#include <utility>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace WallpaperEngine::Render;

CWallpaper::CWallpaper (Core::CWallpaper* wallpaperData, std::string type, CContainer* container) :
    m_container (container),
    m_wallpaperData (wallpaperData),
    m_type (std::move(type))
{
    this->setupFramebuffers ();
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
    return this->m_sceneFramebuffer;
}

GLuint CWallpaper::getWallpaperTexture () const
{
    return this->m_sceneTexture;
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

void CWallpaper::render (glm::vec4 viewport, bool newFrame)
{
    if (newFrame == true)
        this->renderFrame ();

    int windowWidth = 1920;
    int windowHeight = 1080;

    if (this->getWallpaperData ()->is <WallpaperEngine::Core::CScene> ())
    {
        auto projection = this->getWallpaperData ()->as <WallpaperEngine::Core::CScene> ()->getOrthogonalProjection ();

        windowWidth = projection->getWidth ();
        windowHeight = projection->getHeight ();
    }
    // TODO: SUPPORT VIDEO
    float widthRatio = windowWidth / viewport.z;
    float heightRatio = windowHeight / viewport.w;

    GLfloat position [] = {
        widthRatio * -1.0f, heightRatio * 1.0f, 0.0f,
        widthRatio * 1.0f, heightRatio * 1.0f, 0.0f,
        widthRatio * -1.0f, heightRatio * -1.0f, 0.0f,
        widthRatio * -1.0f, heightRatio * -1.0f, 0.0f,
        widthRatio * 1.0f, heightRatio * 1.0f, 0.0f,
        widthRatio * 1.0f, heightRatio * -1.0f, 0.0f
    };

    glBindBuffer (GL_ARRAY_BUFFER, this->m_positionBuffer);
    glBufferData (GL_ARRAY_BUFFER, sizeof (position), position, GL_STATIC_DRAW);

    /*
        -1.0f, 1.0f, 0.0f,
        1.0, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f
     */

    glViewport (viewport.x, viewport.y, viewport.z, viewport.w);

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
}

void CWallpaper::pinpongFramebuffer (GLuint* drawTo, GLuint* inputTexture)
{
    // get current main framebuffer and texture so we can swap them
    GLuint currentMainFramebuffer = this->m_mainFramebuffer;
    GLuint currentMainTexture = this->m_mainTexture;
    GLuint currentSubFramebuffer = this->m_subFramebuffer;
    GLuint currentSubTexture = this->m_subTexture;

    if (drawTo != nullptr)
        *drawTo = currentSubFramebuffer;
    if (inputTexture != nullptr)
        *inputTexture = currentMainTexture;

    // swap the textures
    this->m_mainFramebuffer = currentSubFramebuffer;
    this->m_mainTexture = currentSubTexture;
    this->m_subFramebuffer = currentMainFramebuffer;
    this->m_subTexture = currentMainTexture;
}

void CWallpaper::createFramebuffer (GLuint* framebuffer, GLuint* depthbuffer, GLuint* texture)
{
    int windowWidth = 1920;
    int windowHeight = 1080;

    if (this->getWallpaperData ()->is <WallpaperEngine::Core::CScene> ())
    {
        auto projection = this->getWallpaperData ()->as <WallpaperEngine::Core::CScene> ()->getOrthogonalProjection ();

        windowWidth = projection->getWidth ();
        windowHeight = projection->getHeight ();
    }
    // TODO: SUPPORT VIDEO

    GLenum drawBuffers [1] = {GL_COLOR_ATTACHMENT0};
    // create the main framebuffer
    glGenFramebuffers (1, framebuffer);
    glBindFramebuffer (GL_FRAMEBUFFER, *framebuffer);
    // create the main texture
    glGenTextures (1, texture);
    // bind the new texture to set settings on it
    glBindTexture (GL_TEXTURE_2D, *texture);
    // give OpenGL an empty image
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, windowWidth, windowHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // set filtering parameters, otherwise the texture is not rendered
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // create the depth render buffer for the main framebuffer
    glGenRenderbuffers (1, depthbuffer);
    glBindRenderbuffer (GL_RENDERBUFFER, *depthbuffer);
    glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT, windowWidth, windowHeight);
    glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, *depthbuffer);
    // set the texture as the colour attachmend #0
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *texture, 0);
    // finally set the list of draw buffers
    glDrawBuffers (1, drawBuffers);

    // ensure first framebuffer is okay
    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error ("Framebuffers are not properly set");
}

void CWallpaper::setupFramebuffers ()
{
    this->createFramebuffer (&this->m_mainFramebuffer, &this->m_mainDepthBuffer, &this->m_mainTexture);
    this->createFramebuffer (&this->m_subFramebuffer, &this->m_subDepthBuffer, &this->m_subTexture);
    // create framebuffer for the scene
    this->createFramebuffer (&this->m_sceneFramebuffer, &this->m_sceneDepthBuffer, &this->m_sceneTexture);
}