#pragma once

#include "Helpers/ContextAware.h"
#include "TextureProvider.h"
#include "WallpaperEngine/Media/MediaSource.h"

namespace WallpaperEngine::Render {
class AlbumTexture : public TextureProvider, public Helpers::ContextAware {
public:
    explicit AlbumTexture (RenderContext& context);
    ~AlbumTexture () override;

    [[nodiscard]] GLuint getTextureID (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getTextureWidth (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getTextureHeight (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getRealWidth () const override;
    [[nodiscard]] uint32_t getRealHeight () const override;
    [[nodiscard]] TextureFormat getFormat () const override;
    [[nodiscard]] uint32_t getFlags () const override;
    [[nodiscard]] const std::vector<FrameSharedPtr>& getFrames () const override;
    [[nodiscard]] const glm::vec4* getResolution () const override;
    [[nodiscard]] bool isAnimated () const override;
    [[nodiscard]] uint32_t getSpritesheetCols () const override;
    [[nodiscard]] uint32_t getSpritesheetRows () const override;
    [[nodiscard]] uint32_t getSpritesheetFrames () const override;
    [[nodiscard]] float getSpritesheetDuration () const override;

    void incrementUsageCount () const override;
    void decrementUsageCount () const override;
    void update () const override;

    void copyContents (const TextureProvider& other) const noexcept;
    void load () const;
    bool isReady () const override;

private:
    std::vector<FrameSharedPtr> m_frames;
    mutable glm::vec4 m_resolution;
    mutable uint32_t m_width = 0;
    mutable uint32_t m_height = 0;
    GLuint m_textureID = GL_NONE;
};
}