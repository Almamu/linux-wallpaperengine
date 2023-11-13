#pragma once

#include "WallpaperEngine/Core/CVideo.h"

#include "WallpaperEngine/Audio/CAudioStream.h"
#include "WallpaperEngine/Render/CWallpaper.h"
#include <mpv/client.h>
#include <mpv/render_gl.h>

namespace WallpaperEngine::Render
{
    class CVideo : public CWallpaper
    {
    public:
        CVideo (Core::CVideo* video, CRenderContext& context, CAudioContext& audioContext, const CWallpaperState::TextureUVsScaling& scalingMode);

        Core::CVideo* getVideo ();

        uint32_t getWidth () const override;
        uint32_t getHeight () const override;

        void setSize (int64_t width, int64_t height);

    protected:
        void renderFrame (glm::ivec4 viewport) override;

        friend class CWallpaper;

        static const std::string Type;

    private:
        mpv_handle* m_mpv;
        mpv_render_context* m_mpvGl;

        int64_t m_width;
        int64_t m_height;
    };
}
