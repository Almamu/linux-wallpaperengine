#pragma once

#include <GL/glew.h>
#include <array>
#include <GLFW/glfw3.h>
#include <optional>
#include <utility>

#include "WallpaperEngine/Audio/AudioContext.h"

#include "WallpaperEngine/Render/CFBO.h"
#include "WallpaperEngine/Render/TransitionMode.h"
#include "WallpaperEngine/Render/Helpers/ContextAware.h"
#include "WallpaperEngine/Render/RenderContext.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"
#include "WallpaperEngine/Media/MediaSource.h"

#include "FBOProvider.h"
#include "WallpaperState.h"

namespace WallpaperEngine::Application {
class WallpaperApplication;
}

namespace WallpaperEngine::WebBrowser {
class WebBrowserContext;
}

namespace WallpaperEngine::Render {
namespace Helpers {
    class ContextAware;
}

using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Audio;
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::FileSystem;

class CWallpaper : public Helpers::ContextAware, public FBOProvider, public TypeCaster {
    friend class WallpaperEngine::Application::WallpaperApplication;

public:
    /** Information for span-mode rendering: one wallpaper across multiple viewports */
    struct SpanInfo {
	/** Bounding box of the entire span group (x, y, width, height) in global desktop coordinates */
	glm::ivec4 totalBounds;
    };

    virtual ~CWallpaper () override;

    /**
     * Performs a render pass of the wallpaper.
     */
    void render (
	const glm::ivec4& viewport, const bool vflip, const glm::ivec2& globalPosition = { 0, 0 },
	const glm::ivec2& logicalSize = { 0, 0 }, const glm::ivec2& presentationSize = { 0, 0 }
    );

    /**
     * Pause the renderer
     */
    void renderWithColorSpace (
        const glm::ivec4& viewport, bool vflip, const glm::ivec2& globalPosition,
        const glm::ivec2& logicalSize, bool scRGBPresentation, float hdrHeadroom
    );
    virtual void setPause (bool newState);
    /** Decoded HDR input, independent of whether this output can present it. */
    [[nodiscard]] virtual bool hasHDRSource () const { return false; }
    /** Called once before the primary output frame, not by secondary SDR captures. */
    virtual void prepareHDRPresentation (bool enabled) { }

    /**
     * @return The container to resolve files for this wallpaper
     */
    [[nodiscard]] const AssetLocator& getAssetLocator () const;

    /**
     * @return The current audio context for this wallpaper
     */
    AudioContext& getAudioContext () const;

    /**
     * @return The wallpaper state
     */
    [[nodiscard]] const WallpaperState& getState () const;

    /**
     * @return The scene's framebuffer
     */
    [[nodiscard]] virtual GLuint getWallpaperFramebuffer () const;
    /**
     * @return The scene's texture
     */
    [[nodiscard]] virtual GLuint getWallpaperTexture () const;
    /**
     * Searches the FBO list for the given FBO
     *
     * @param name
     * @return
     */
    [[nodiscard]] std::shared_ptr<const CFBO> findFBO (const std::string& name) const;

    /**
     * @return The main FBO of this wallpaper
     */
    [[nodiscard]] std::shared_ptr<const CFBO> getFBO () const;
    /** Actual readback dimensions; video/web can resize GL storage independently. */
    [[nodiscard]] virtual glm::ivec2 getFramebufferSize () const;
    /** Scene framing is already rasterized; video/web still scale during presentation. */
    [[nodiscard]] virtual WallpaperState::TextureUVs getPresentationUVs () const;

    /**
     * Updates the UVs coordinates if window/screen/vflip/projection has changed
     */
    void updateUVs (const glm::ivec4& viewport, const bool vflip);

    /**
     * Updates the destination framebuffer for this wallpaper
     *
     * @param framebuffer
     */
    void setDestinationFramebuffer (GLuint framebuffer);
    /** Negative FPS hides the overlay; nonnegative values are measured per output. */
    void setFpsOverlay (float fps) { m_fps = fps; }
    /** A negative total hides the load-time HUD; details add the phases that contributed. */
    void setLoadOverlay (const std::array<float, 7>& seconds, bool details) {
	m_loadTimes = seconds;
	m_loadDetails = details;
    }

    /**
     * Transition state for the final on-screen composite; while progress < 1.0 the
     * composite shader reveals this wallpaper over the outgoing one using the given mode.
     * @param center normalized origin for transitions that grow from a point (e.g. disc)
     */
    void setTransition (TransitionMode mode, float progress, glm::vec2 center = { 0.5f, 0.5f });

    /**
     * Sets span info for this wallpaper, enabling span-mode rendering
     */
    void setSpanInfo (const SpanInfo& spanInfo);

    /**
     * @return The span info if set, or nullptr
     */
    [[nodiscard]] const SpanInfo* getSpanInfo () const;

    /**
     * @return The width of this wallpaper
     */
    [[nodiscard]] virtual int getWidth () const = 0;

    /**
     * @return The height of this wallpaper
     */
    [[nodiscard]] virtual int getHeight () const = 0;

    /**
     * Creates a new instance of CWallpaper based on the information provided by the read backgrounds
     *
     * @param wallpaper
     * @param context
     * @param audioContext
     * @param scalingMode
     *
     * @return
     */
    static std::unique_ptr<CWallpaper> fromWallpaper (
	const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
	WebBrowser::WebBrowserContext* browserContext, const WallpaperState::TextureUVsScaling& scalingMode,
	const uint32_t& clampMode, const std::string& screenName = "default"
    );

protected:
    CWallpaper (
	const Wallpaper& wallpaperData, RenderContext& context, AudioContext& audioContext,
	const WallpaperState::TextureUVsScaling& scalingMode, const uint32_t& clampMode
    );

    /**
     * Renders a frame of the wallpaper
     */
    virtual void renderFrame (const glm::ivec4& viewport) = 0;

    /**
     * Setups OpenGL's framebuffers for ping-pong and scene rendering
     */
    void setupFramebuffers (bool sceneDepthBuffer = false, glm::vec2 size = { 0, 0 }, uint32_t samples = 1,
	TextureFormat format = TextureFormat_ARGB8888, bool reverseDepth = false);

    const Wallpaper& m_wallpaperData;

    [[nodiscard]] const Wallpaper& getWallpaperData () const;

    /** The FBO used for scene output */
    std::shared_ptr<const CFBO> m_sceneFBO = nullptr;

    GLuint m_vaoBuffer = GL_NONE;
    bool m_firstFramePrepared = false;

    /** Optional scene-local fade composited over the rendered wallpaper. */
    [[nodiscard]] virtual float getSceneFadeAlpha () const;
    /** Zero for existing SDR bytes; otherwise linear RGB multiplier to scRGB/80 nits. */
    [[nodiscard]] virtual float getHDRSourceScale () const { return 0.0f; }
    [[nodiscard]] virtual glm::vec4 getColorCorrection () const;
    [[nodiscard]] virtual std::pair<GLuint, float> getColorLut () const;

private:
    /** The texture used for the scene output */
    GLuint m_texCoordBuffer = GL_NONE;
    GLuint m_positionBuffer = GL_NONE;
    GLuint m_shader = GL_NONE;
    // shader variables
    GLint g_Texture0 = GL_NONE;
    GLint g_TransitionMode = GL_NONE;
    GLint g_TransitionProgress = GL_NONE;
    GLint g_TransitionCenter = GL_NONE;
    GLint g_SceneFadeAlpha = GL_NONE;
    GLint g_Contrast = GL_NONE;
    GLint g_Saturation = GL_NONE;
    bool m_scRGBPresentation = false;
    float m_hdrHeadroom = 1.0f;
    GLint g_ColorCorrection = GL_NONE;
    GLint g_HDRSourceScale = GL_NONE;
    GLint g_ScRGBPresentation = GL_NONE;
    GLint g_HDRHeadroom = GL_NONE;
    GLint g_ColorLut = GL_NONE;
    GLint g_ColorLutStrength = GL_NONE;
    GLint g_Fps = GL_NONE;
    GLint g_FpsOutput = GL_NONE;
    float m_fps = -1.0f;
    GLint g_LoadTimes = GL_NONE;
    GLint g_LoadDetails = GL_NONE;
    // RenderContext::LoadTimes; spelled out because RenderContext.h includes this header.
    std::array<float, 7> m_loadTimes { -1.0f };
    bool m_loadDetails = false;
    GLint a_Position = GL_NONE;
    GLint a_TexCoord = GL_NONE;
    /** The framebuffer to draw the background to */
    GLuint m_destFramebuffer = GL_NONE;
    /** Reveal animation used while compositing the scene FBO to the screen */
    TransitionMode m_transitionMode = TransitionMode_None;
    /** Progress of the reveal animation, 0..1 */
    float m_transitionProgress = 1.0f;
    /** Normalized origin for point-based reveal animations */
    glm::vec2 m_transitionCenter = { 0.5f, 0.5f };
    /** Setups OpenGL's shaders for this wallpaper backbuffer */
    void setupShaders ();
    /** List of FBOs registered for this wallpaper */
    std::map<std::string, std::shared_ptr<const CFBO>> m_fbos = {};
    /** Audio context that is using this wallpaper */
    AudioContext& m_audioContext;
    /** Current Wallpaper state */
    WallpaperState m_state;
    /** Neutral-at-1.0 final-output color controls shared by scene/video/web wallpapers. */
    float m_contrast = 1.0f;
    float m_saturation = 1.0f;
    /** Span info for multi-monitor spanning (optional) */
    std::optional<SpanInfo> m_spanInfo = std::nullopt;
    /** Frame counter to avoid redundant renderFrame calls when shared across viewports */
    uint32_t m_lastRenderedFrame = UINT32_MAX;
};
} // namespace WallpaperEngine::Render
