#pragma once

#include "WallpaperEngine/Audio/CAudioStream.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include <mpv/client.h>
#include <mpv/render_gl.h>

namespace WallpaperEngine::Render::Wallpapers {
class CVideo final : public CWallpaper {
  public:
    CVideo (
        const Wallpaper& video, CRenderContext& context, CAudioContext& audioContext,
        const CWallpaperState::TextureUVsScaling& scalingMode,
        const WallpaperEngine::Assets::ITexture::TextureFlags& clampMode);

    const Video& getVideo () const;

    [[nodiscard]] int getWidth () const override;
    [[nodiscard]] int getHeight () const override;

    void setPause (bool newState) override;
    void setSize (int width, int height);

  protected:
    void renderFrame (glm::ivec4 viewport) override;

    friend class CWallpaper;

  private:
    mpv_handle* m_mpv = nullptr;
    mpv_render_context* m_mpvGl = nullptr;

    bool m_paused = false;
    int64_t m_width = 16;
    int64_t m_height = 16;
};
} // namespace WallpaperEngine::Render::Wallpapers
