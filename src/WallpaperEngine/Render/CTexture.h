#pragma once

#include "Helpers/ContextAware.h"
#include "TextureProvider.h"
#include "WallpaperEngine/Data/Assets/Texture.h"
#include "WallpaperEngine/VideoPlayback/MPV/GLPlayer.h"

#include <GL/glew.h>
#include <glm/vec4.hpp>
#include <memory>
#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>
#include <vector>

namespace WallpaperEngine::Render {
class RenderContext;
using namespace WallpaperEngine::Data::Assets;
using namespace WallpaperEngine::VideoPlayback::MPV;
/**
 * A normal texture file in WallpaperEngine's format
 */
class CTexture final : public TextureProvider, public Helpers::ContextAware {
public:
    explicit CTexture (RenderContext& context, TextureUniquePtr header);
    ~CTexture () override;

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
    [[nodiscard]] uint32_t getSpritesheetCols () const override;
    [[nodiscard]] uint32_t getSpritesheetRows () const override;
    [[nodiscard]] uint32_t getSpritesheetFrames () const override;
    [[nodiscard]] float getSpritesheetDuration () const override;

    /**
     * Increments the usage count of the texture
     *
     * Directly controls playback for video CTextures, only started when at least one thing is using it
     * Initializes mpv if needed and starts playback
     */
    void incrementUsageCount () const override;
    /**
     * Decrements the usage count of the texture
     *
     * Directly controls playback for video CTextures, only stopped when nothing is using it
     * De-initializes mpv if needed
     */
    void decrementUsageCount () const override;
    /**
     * Some textures need to be updated
     */
    void update () const override;

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
    void setupOpenGLParameters (uint32_t textureID) const;

    /** The texture header */
    TextureUniquePtr m_header;
    /** OpenGL's texture ID */
    GLuint* m_textureID = nullptr;
    /** Resolution vector of the texture */
    glm::vec4 m_resolution {};
    /** The video player in use */
    GLPlayerUniquePtr m_player;
};
} // namespace WallpaperEngine::Assets