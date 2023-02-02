#include "common.h"
#include "CFBO.h"

using namespace WallpaperEngine::Render;

CFBO::CFBO (
    std::string name,
    ITexture::TextureFormat format,
    ITexture::TextureFlags flags,
    float scale,
    uint32_t realWidth, uint32_t realHeight,
    uint32_t textureWidth, uint32_t textureHeight
) :
    m_name (std::move (name)),
    m_format (format),
    m_scale (scale),
    m_flags (flags)
{
    // create an empty texture that'll be free'd so the FBO is transparent
    GLenum drawBuffers [1] = {GL_COLOR_ATTACHMENT0};
    // create the main framebuffer
    glGenFramebuffers (1, &this->m_framebuffer);
    glBindFramebuffer (GL_FRAMEBUFFER, this->m_framebuffer);
    // create the main texture
    glGenTextures (1, &this->m_texture);
    // bind the new texture to set settings on it
    glBindTexture (GL_TEXTURE_2D, this->m_texture);
    // give OpenGL an empty image
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, textureWidth, textureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    // label stuff for debugging
    if (DEBUG)
        glObjectLabel (GL_TEXTURE, this->m_texture, -1, this->m_name.c_str ());

    // set filtering parameters, otherwise the texture is not rendered
    if (flags & TextureFlags::ClampUVs)
    {
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else
    {
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    if (flags & TextureFlags::NoInterpolation)
    {
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
    else
    {
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }

    glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 8.0f);

    // set the texture as the colour attachmend #0
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->m_texture, 0);
    // finally set the list of draw buffers
    glDrawBuffers (1, drawBuffers);

    // ensure first framebuffer is okay
    if (glCheckFramebufferStatus (GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        sLog.exception ("Framebuffers are not properly set");

    // clear the framebuffer
    glClear (GL_COLOR_BUFFER_BIT);

    this->m_resolution = {
        textureWidth, textureHeight,
        realWidth, realHeight
    };

    // create the textureframe entries
    TextureFrame* frame = new TextureFrame;

    frame->frameNumber = 0;
    frame->frametime = 0;
    frame->height1 = textureHeight;
    frame->height2 = realHeight;
    frame->width1 = textureWidth;
    frame->width2 = realWidth;
    frame->x = 0;
    frame->y = 0;

    this->m_frames.push_back (frame);
}

const std::string& CFBO::getName () const
{
    return this->m_name;
}

const float& CFBO::getScale () const
{
    return this->m_scale;
}

const ITexture::TextureFormat CFBO::getFormat () const
{
    return this->m_format;
}

const ITexture::TextureFlags CFBO::getFlags () const
{
    return this->m_flags;
}

GLuint CFBO::getFramebuffer () const
{
    return this->m_framebuffer;
}

GLuint CFBO::getDepthbuffer () const
{
    return this->m_depthbuffer;
}

const GLuint CFBO::getTextureID (uint32_t imageIndex) const
{
    return this->m_texture;
}

const uint32_t CFBO::getTextureWidth (uint32_t imageIndex) const
{
    return this->m_resolution.x;
}
const uint32_t CFBO::getTextureHeight (uint32_t imageIndex) const
{
    return this->m_resolution.y;
}

const uint32_t CFBO::getRealWidth () const
{
    return this->m_resolution.z;
}

const uint32_t CFBO::getRealHeight () const
{
    return this->m_resolution.w;
}

const std::vector<ITexture::TextureFrame*>& CFBO::getFrames () const
{
    return this->m_frames;
}

const glm::vec4* CFBO::getResolution () const
{
    return &this->m_resolution;
}

const bool CFBO::isAnimated () const
{
    return false;
}