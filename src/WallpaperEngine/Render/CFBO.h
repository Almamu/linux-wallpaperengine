#pragma once

#include <atomic>
#include <string>
#include <string_view>

#include "TextureProvider.h"

using namespace WallpaperEngine::Render;

namespace WallpaperEngine::Render {
class CFBO final : public TextureProvider {
public:
    CFBO (
	std::string name, const TextureFormat format, const uint32_t flags, const float scale, uint32_t realWidth,
	uint32_t realHeight, uint32_t textureWidth, uint32_t textureHeight, bool withDepthBuffer = false,
	bool depthTexture = false, uint32_t requestedSamples = 1, bool reverseDepth = false
    );
    ~CFBO () override;

    [[nodiscard]] const std::string& getName () const;
    [[nodiscard]] const float& getScale () const;
    [[nodiscard]] TextureFormat getFormat () const override;
    [[nodiscard]] uint32_t getFlags () const override;
    [[nodiscard]] GLuint getFramebuffer () const;
    [[nodiscard]] GLuint getDrawFramebuffer () const;
    [[nodiscard]] GLuint getDepthbuffer () const;
    [[nodiscard]] uint32_t getSamples () const;
    [[nodiscard]] uint32_t getDrawSamples () const { return m_multisampleRendering ? m_samples : 1; }
    [[nodiscard]] bool usesReverseDepth () const { return m_reverseDepth; }
    [[nodiscard]] float getFarDepth () const { return m_reverseDepth ? 0.0f : 1.0f; }
    [[nodiscard]] GLenum getDepthInternalFormat () const;
    void setMultisampleRendering (bool enabled);
    /**
     * Regenerates the mip chain from level 0 for Wallpaper Engine's dedicated
     * _rt_MipMappedFrameBuffer target (a no-op for ordinary layer composites).
     */
    void generateMipmaps () const;
    /** Clear color storage in the calling context, including script initialization. */
    void clear (const glm::vec4& color) const;
    /** Resize storage on the render thread while preserving references and GL handles. */
    void resize (uint32_t width, uint32_t height);
    [[nodiscard]] GLuint getTextureID (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getTextureWidth (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getTextureHeight (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getMipMapCount (uint32_t imageIndex) const override;
    [[nodiscard]] uint32_t getRealWidth () const override;
    [[nodiscard]] uint32_t getRealHeight () const override;
    [[nodiscard]] const std::vector<FrameSharedPtr>& getFrames () const override;
    [[nodiscard]] const glm::vec4* getResolution () const override;
    [[nodiscard]] bool isAnimated () const override;
    [[nodiscard]] size_t getApproximateGpuBytes () const override;
    [[nodiscard]] uint32_t getSpritesheetCols () const override;
    [[nodiscard]] uint32_t getSpritesheetRows () const override;
    [[nodiscard]] uint32_t getSpritesheetFrames () const override;
    [[nodiscard]] float getSpritesheetDuration () const override;

    void incrementUsageCount () const override;
    void decrementUsageCount () const override;
    void update () const override;
    bool isReady () const override;

    [[nodiscard]] static size_t getLiveCount ();
    [[nodiscard]] static size_t getLiveGpuBytes ();
    /** Wallpaper Engine generates mip levels only for this named reflection target. */
    [[nodiscard]] static bool targetUsesMipmaps (std::string_view name);
    /** Largest live render-target groups, for the control socket's fbostats command. */
    [[nodiscard]] static std::string getLiveDebugSummary (size_t limit = 20);

private:
    /**
     * Creates the framebuffer object on first use. Framebuffers are container objects
     * and are NOT shared between GL contexts: when a wallpaper is built on the async
     * switch worker's shared context, only the texture/renderbuffer can be created
     * there — the FBO itself must be created lazily on the render thread.
     */
    void ensureFramebuffer () const;
    void ensureMultisampleFramebuffer () const;
    void resolveMultisample () const;

    /** Only the dedicated reflection target carries a mip chain. */
    [[nodiscard]] bool hasMipmaps () const;
    void configureMipmaps (uint32_t width, uint32_t height);
    void clearTextureStorage (const glm::vec4& color = glm::vec4 (0.0f)) const;
    [[nodiscard]] size_t calculateStorageBytes (uint32_t width, uint32_t height) const;

    mutable bool m_storageCleared = false;
    mutable GLuint m_framebuffer = GL_NONE;
    mutable GLuint m_multisampleFramebuffer = GL_NONE;
    GLuint m_depthbuffer = GL_NONE;
    GLuint m_multisampleColor = GL_NONE;
    GLuint m_multisampleDepth = GL_NONE;
    GLuint m_texture = GL_NONE;
    uint32_t m_samples = 1;
    uint32_t m_mipMapCount = 1;
    size_t m_approximateGpuBytes = 0;
    /** The main texture is a sampleable depth-comparison attachment, not RGBA color. */
    bool m_depthTexture = false;
    bool m_withDepthBuffer = false;
    bool m_reverseDepth = false;
    bool m_multisampleRendering = false;
    glm::vec4 m_resolution = {};
    float m_scale = 0;
    std::string m_name = "";
    TextureFormat m_format = TextureFormat_UNKNOWN;
    uint32_t m_flags = TextureFlags_NoFlags;
    /** Placeholder for frames, FBOs only have ONE */
    std::vector<FrameSharedPtr> m_frames = {};
    static std::atomic<size_t> s_liveCount;
    static std::atomic<size_t> s_liveGpuBytes;
};
} // namespace WallpaperEngine::Render
