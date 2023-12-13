#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "WallpaperEngine/Assets/CContainer.h"
#include "WallpaperEngine/Audio/CAudioContext.h"

#include "WallpaperEngine/Core/CScene.h"
#include "WallpaperEngine/Core/CVideo.h"
#include "WallpaperEngine/Core/CWallpaper.h"

#include "WallpaperEngine/Render/CFBO.h"
#include "WallpaperEngine/Render/CRenderContext.h"
#include "WallpaperEngine/Render/Helpers/CContextAware.h"

#include "CWallpaperState.h"

using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Audio;

namespace WallpaperEngine::Render {
namespace Helpers {
class CContextAware;
}

class CWallpaper : public Helpers::CContextAware {
  public:
    template <class T> const T* as () const {
        assert (is<T> ());
        return reinterpret_cast<const T*> (this);
    }

    template <class T> T* as () {
        assert (is<T> ());
        return reinterpret_cast<T*> (this);
    }

    template <class T> bool is () {
        return this->m_type == T::Type;
    }

    ~CWallpaper () override;

    /**
     * Performs a render pass of the wallpaper
     */
    void render (glm::ivec4 viewport, bool vflip);

    /**
     * @return The container to resolve files for this wallpaper
     */
    [[nodiscard]] CContainer* getContainer () const;

    /**
     * @return The current audio context for this wallpaper
     */
    CAudioContext& getAudioContext ();

    /**
     * @return The scene's framebuffer
     */
    [[nodiscard]] virtual GLuint getWallpaperFramebuffer () const;
    /**
     * @return The scene's texture
     */
    [[nodiscard]] virtual GLuint getWallpaperTexture () const;
    /**
     * Creates a new FBO for this wallpaper
     *
     * @param name The name of the FBO
     * @param format
     * @param flags
     * @param scale
     * @param realWidth
     * @param realHeight
     * @param textureWidth
     * @param textureHeight
     * @return
     */
    CFBO* createFBO (const std::string& name, ITexture::TextureFormat format, ITexture::TextureFlags flags, float scale,
                     uint32_t realWidth, uint32_t realHeight, uint32_t textureWidth, uint32_t textureHeight);

    /**
     * @return The full FBO list to work with
     */
    [[nodiscard]] const std::map<std::string, CFBO*>& getFBOs () const;
    /**
     * Searches the FBO list for the given FBO
     *
     * @param name
     * @return
     */
    [[nodiscard]] CFBO* findFBO (const std::string& name) const;

    /**
     * @return The main FBO of this wallpaper
     */
    [[nodiscard]] CFBO* getFBO () const;

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
    [[nodiscard]] virtual uint32_t getWidth () const = 0;

    /**
     * @return The height of this wallpaper
     */
    [[nodiscard]] virtual uint32_t getHeight () const = 0;

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
    static CWallpaper* fromWallpaper (Core::CWallpaper* wallpaper, CRenderContext& context, CAudioContext& audioContext,
                                      const CWallpaperState::TextureUVsScaling& scalingMode);

  protected:
    CWallpaper (Core::CWallpaper* wallpaperData, std::string type, CRenderContext& context, CAudioContext& audioContext,
                const CWallpaperState::TextureUVsScaling& scalingMode);

    /**
     * Renders a frame of the wallpaper
     */
    virtual void renderFrame (glm::ivec4 viewport) = 0;

    /**
     * Setups OpenGL's framebuffers for ping-pong and scene rendering
     */
    void setupFramebuffers ();

    Core::CWallpaper* m_wallpaperData;

    [[nodiscard]] Core::CWallpaper* getWallpaperData () const;

    /** The FBO used for scene output */
    CFBO* m_sceneFBO;

  private:
    /** The texture used for the scene output */
    GLuint m_texCoordBuffer;
    GLuint m_positionBuffer;
    GLuint m_shader;
    // shader variables
    GLint g_Texture0;
    GLint a_Position;
    GLint a_TexCoord;
    GLuint m_vaoBuffer;
    /** The framebuffer to draw the background to */
    GLuint m_destFramebuffer;
    /** Setups OpenGL's shaders for this wallpaper backbuffer */
    void setupShaders ();
    /** The type of background this wallpaper is */
    std::string m_type;
    /** List of FBOs registered for this wallpaper */
    std::map<std::string, CFBO*> m_fbos;
    /** Audio context that is using this wallpaper */
    CAudioContext& m_audioContext;
    /** Current Wallpaper state */
    CWallpaperState m_state;
};
} // namespace WallpaperEngine::Render
