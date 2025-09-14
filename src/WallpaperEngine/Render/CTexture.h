#pragma once

#include "TextureProvider.h"
#include "WallpaperEngine/Data/Assets/Texture.h"

#include <GL/glew.h>
#include <glm/vec4.hpp>
#include <memory>
#include <vector>

namespace WallpaperEngine::Render {
using namespace WallpaperEngine::Data::Assets;

/**
 * A normal texture file in WallpaperEngine's format
 */
class CTexture final : public TextureProvider {
  public:
    explicit CTexture (TextureUniquePtr header);

    [[nodiscard]] GLuint getTextureID (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getTextureWidth (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getTextureHeight (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getRealWidth () const override;
    [[nodiscard]] uint32_t getRealHeight () const override;
    [[nodiscard]] TextureFormat getFormat () const override;
    [[nodiscard]] uint32_t getFlags () const override;
    [[nodiscard]] const glm::vec4* getResolution () const override;
    [[nodiscard]] const std::vector<FrameSharedPtr>& getFrames () const override;
    [[nodiscard]] bool isAnimated () const override;

  private:
    /**
     * @return The texture header
     */
    [[nodiscard]] const Texture& getHeader () const;

    /**
     * Calculate's texture's resolution vec4
     */
    void setupResolution ();
    /**
     * Determines the texture's internal storage format
     */
    GLint setupInternalFormat () const;
    /**
     * Prepares openGL parameters for loading texture data
     */
    void setupOpenGLParameters (const uint32_t textureID) const;

    /** The texture header */
    TextureUniquePtr m_header;
    /** OpenGL's texture ID */
    GLuint* m_textureID = nullptr;
    /** Resolution vector of the texture */
    glm::vec4 m_resolution {};
};
} // namespace WallpaperEngine::Assets