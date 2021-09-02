#include "CWallpaper.h"

#include <utility>
#include <glm/glm.hpp>

using namespace WallpaperEngine::Render;

CWallpaper::CWallpaper (Core::CWallpaper* wallpaperData, std::string type, CContainer* container) :
    m_container (container),
    m_wallpaperData (wallpaperData),
    m_type (std::move(type))
{
    this->setupFramebuffers ();
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

void CWallpaper::setupFramebuffers ()
{
    // TODO: ENSURE THE WINDOW WIDTH AND HEIGHT IS CORRECT AND UPDATE THEM WHEN THE SCREEN CHANGES
    GLenum drawBuffers [1] = {GL_COLOR_ATTACHMENT0};
    // create the main framebuffer
    glGenFramebuffers (1, &this->m_mainFramebuffer);
    glBindFramebuffer (GL_FRAMEBUFFER, this->m_mainFramebuffer);
    // create the main texture
    glGenTextures (1, &this->m_mainTexture);
    // bind the new texture to set settings on it
    glBindTexture (GL_TEXTURE_2D, this->m_mainTexture);
    // give OpenGL an empty image
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // set filtering parameters, otherwise the texture is not rendered
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // create the depth render buffer for the main framebuffer
    glGenRenderbuffers (1, &this->m_mainDepthBuffer);
    glBindRenderbuffer (GL_RENDERBUFFER, this->m_mainDepthBuffer);
    glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1920, 1080);
    glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, this->m_mainDepthBuffer);
    // set the texture as the colour attachmend #0
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->m_mainTexture, 0);
    // finally set the list of draw buffers
    glDrawBuffers (1, drawBuffers);

    // ensure first framebuffer is okay
    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error ("Framebuffers are not properly set");

    // create the sub framebuffer
    glGenFramebuffers (1, &this->m_subFramebuffer);
    glBindFramebuffer (GL_FRAMEBUFFER, this->m_subFramebuffer);
    // create the sub texture
    glGenTextures (1, &this->m_subTexture);
    // bind the new texture to set settings on it
    glBindTexture (GL_TEXTURE_2D, this->m_subTexture);
    // give OpenGL an empty image
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    // set filtering parameters, otherwise the texture is not rendered
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // create the depth render buffer for the main framebuffer
    glGenRenderbuffers (1, &this->m_subDepthBuffer);
    glBindRenderbuffer (GL_RENDERBUFFER, this->m_subDepthBuffer);
    glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT, 1920, 1080);
    glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, this->m_subDepthBuffer);
    // set the texture as the colour attachmend #0
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->m_subTexture, 0);
    // finally set the list of draw buffers
    glDrawBuffers (1, drawBuffers);

    // ensure second framebuffer is okay
    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error ("Framebuffers are not properly set");
}