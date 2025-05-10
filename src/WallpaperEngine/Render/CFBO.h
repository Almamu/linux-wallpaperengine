#pragma once

#include "WallpaperEngine/Assets/ITexture.h"
#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/Effects/CFBO.h"

using namespace WallpaperEngine::Assets;

namespace WallpaperEngine::Render {
class CFBO final : public ITexture {
  public:
    CFBO (std::string name, ITexture::TextureFormat format, ITexture::TextureFlags flags, float scale,
          uint32_t realWidth, uint32_t realHeight, uint32_t textureWidth, uint32_t textureHeight);
    ~CFBO () override;

    // TODO: ADD DESTRUCTOR TO FREE RESOURCES

    [[nodiscard]] const std::string& getName () const;
    [[nodiscard]] const float& getScale () const;
    [[nodiscard]] ITexture::TextureFormat getFormat () const override;
    [[nodiscard]] ITexture::TextureFlags getFlags () const override;
    [[nodiscard]] GLuint getFramebuffer () const;
    [[nodiscard]] GLuint getDepthbuffer () const;
    [[nodiscard]] GLuint getTextureID (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getTextureWidth (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getTextureHeight (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getRealWidth () const override;
    [[nodiscard]] uint32_t getRealHeight () const override;
    [[nodiscard]] const std::vector<std::shared_ptr<TextureFrame>>& getFrames () const override;
    [[nodiscard]] const glm::vec4* getResolution () const override;
    [[nodiscard]] bool isAnimated () const override;

  private:
    GLuint m_framebuffer = GL_NONE;
    GLuint m_depthbuffer = GL_NONE;
    GLuint m_texture = GL_NONE;
    glm::vec4 m_resolution = {};
    float m_scale = 0;
    std::string m_name = "";
    ITexture::TextureFormat m_format = UNKNOWN;
    ITexture::TextureFlags m_flags = NoFlags;
    /** Placeholder for frames, FBOs only have ONE */
    std::vector<std::shared_ptr<TextureFrame>> m_frames = {};
};
} // namespace WallpaperEngine::Render
