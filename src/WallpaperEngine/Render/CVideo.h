#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/CVideo.h"

#include "WallpaperEngine/Render/CWallpaper.h"

#include "WallpaperEngine/Irrlicht/CContext.h"

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

namespace WallpaperEngine::Render
{
    class CVideo : public CWallpaper
    {
    public:
        CVideo (Core::CVideo* video, CContainer* container);

        void render () override;

        Core::CVideo* getVideo ();

    protected:
        friend class CWallpaper;

        static const std::string Type;

    private:
        void setSize (int width, int height);
        void restartStream ();
        void getNextFrame ();
        void writeFrameToImage ();

        AVFormatContext* m_formatCtx = nullptr;
        AVCodecContext* m_codecCtx = nullptr;
        AVFrame* m_videoFrame = nullptr;
        AVFrame* m_videoFrameRGB = nullptr;
        SwsContext* m_swsCtx = nullptr;
        uint8_t* m_buffer = nullptr;
        int m_videoStream = -1, m_audioStream = -1;

        irr::video::IImage* m_frameImage;
        irr::video::ITexture* m_frameTexture;
    };
};
