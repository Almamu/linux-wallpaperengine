#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "WallpaperEngine/Audio/AudioContext.h"

#include "WallpaperEngine/Render/CFBO.h"
#include "WallpaperEngine/Render/Helpers/ContextAware.h"
#include "WallpaperEngine/Render/RenderContext.h"

#include "WallpaperEngine/Data/Model/Wallpaper.h"

#include "FBOProvider.h"
#include "WallpaperState.h"

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

class CWallpaper : public Helpers::ContextAware, public FBOProvider {
  public:
    template <class T> [[nodiscard]] const T* as () const {
        if (is <T> ()) {
            return static_cast <const T*> (this);
        }

        throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] T* as () {
        if (is <T> ()) {
            return static_cast <T*> (this);
        }

        throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] bool is () const {
        return typeid (*this) == typeid(T);
    }

    virtual ~CWallpaper () override;

    /**
     * Performs a render pass of the wallpaper
     */
    void render (glm::ivec4 viewport, bool vflip);

    /**
     * Pause the renderer
     */
    virtual void setPause (bool newState);

    /**
     * @return The container to resolve files for this wallpaper
     */
    [[nodiscard]] const AssetLocator& getAssetLocator () const;

    /**
     * @return The current audio context for this wallpaper
     */
    AudioContext& getAudioContext ();

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
        const uint32_t& clampMode);

  protected:
    CWallpaper (
        const Wallpaper& wallpaperData, RenderContext& context,
        AudioContext& audioContext, const WallpaperState::TextureUVsScaling& scalingMode,
        const uint32_t& clampMode);

    /**
     * Renders a frame of the wallpaper
     */
    virtual void renderFrame (glm::ivec4 viewport) = 0;

    /**
     * Setups OpenGL's framebuffers for ping-pong and scene rendering
     */
    void setupFramebuffers ();

    const Wallpaper& m_wallpaperData;

    [[nodiscard]] const Wallpaper& getWallpaperData () const;

    /** The FBO used for scene output */
    std::shared_ptr<const CFBO> m_sceneFBO = nullptr;

  private:
    /** The texture used for the scene output */
    GLuint m_texCoordBuffer = GL_NONE;
    GLuint m_positionBuffer = GL_NONE;
    GLuint m_shader = GL_NONE;
    // shader variables
    GLint g_Texture0 = GL_NONE;
    GLint a_Position = GL_NONE;
    GLint a_TexCoord = GL_NONE;
    GLuint m_vaoBuffer = GL_NONE;
    /** The framebuffer to draw the background to */
    GLuint m_destFramebuffer = GL_NONE;
    /** Setups OpenGL's shaders for this wallpaper backbuffer */
    void setupShaders ();
    /** List of FBOs registered for this wallpaper */
    std::map<std::string, std::shared_ptr<const CFBO>> m_fbos = {};
    /** Audio context that is using this wallpaper */
    AudioContext& m_audioContext;
    /** Current Wallpaper state */
    WallpaperState m_state;
};
} // namespace WallpaperEngine::Render
