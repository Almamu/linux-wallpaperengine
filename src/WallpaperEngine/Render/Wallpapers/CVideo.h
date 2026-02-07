#pragma once

#include "WallpaperEngine/Audio/AudioStream.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include <mpv/client.h>
#include <mpv/render_gl.h>

namespace WallpaperEngine::Render::Wallpapers {
class CVideo final : public CWallpaper {
  public:
    CVideo (
        const Wallpaper& wallpaper, RenderContext& context, AudioContext& audioContext,
        const WallpaperState::TextureUVsScaling& scalingMode,
        const uint32_t& clampMode);

    const Video& getVideo () const;

    [[nodiscard]] int getWidth () const override;
    [[nodiscard]] int getHeight () const override;

    void setPause (bool newState) override;
    void setSize (int width, int height);

  protected:
    void renderFrame (const glm::ivec4& viewport) override;

    friend class CWallpaper;

  private:
    mpv_handle* m_mpv = nullptr;
    mpv_render_context* m_mpvGl = nullptr;

    bool m_paused = false;
    int64_t m_width = 16;
    int64_t m_height = 16;
    bool m_muted = false;
};
} // namespace WallpaperEngine::Render::Wallpapers
