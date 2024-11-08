#pragma once

#include "WallpaperEngine/Core/Wallpapers/CVideo.h"

#include "WallpaperEngine/Audio/CAudioStream.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include <mpv/client.h>
#include <mpv/render_gl.h>

namespace WallpaperEngine::Render {
class CVideo final : public CWallpaper {
  public:
    CVideo (Core::CVideo* video, CRenderContext& context, CAudioContext& audioContext,
            const CWallpaperState::TextureUVsScaling& scalingMode);

    Core::CVideo* getVideo ();

    [[nodiscard]] int getWidth () const override;
    [[nodiscard]] int getHeight () const override;

    void setPause (bool newState) override;
    void setSize (int width, int height);

  protected:
    void renderFrame (glm::ivec4 viewport) override;

    friend class CWallpaper;

    static const std::string Type;

  private:
    mpv_handle* m_mpv;
    mpv_render_context* m_mpvGl;

    bool m_paused;
    int64_t m_width;
    int64_t m_height;
};
} // namespace WallpaperEngine::Render
