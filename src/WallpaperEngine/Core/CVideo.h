#pragma once

#include <irrlicht/irrlicht.h>

#include "Core.h"
#include "CWallpaper.h"

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

namespace WallpaperEngine::Core
{
    class CVideo : public CWallpaper
    {
    public:
        CVideo (
                const irr::io::path& filename
        );

        void setSize (int width, int height);
        void restartStream ();

        AVFormatContext* getFormatContext ();
        AVCodecContext* getCodecContext ();
        AVFrame* getVideoFrame ();
        AVFrame* getVideoFrameRGB ();
        SwsContext* getSwsContext ();
        int getVideoStreamIndex ();
        int getAudioStreamIndex ();

    protected:
        friend class CWallpaper;

        static const std::string Type;

    private:
        AVFormatContext* m_formatCtx = nullptr;
        AVCodecContext* m_codecCtx = nullptr;
        AVFrame* m_videoFrame = nullptr;
        AVFrame* m_videoFrameRGB = nullptr;
        SwsContext* m_swsCtx = nullptr;
        uint8_t* buffer = nullptr;

        int m_videoStream = -1, m_audioStream = -1;
    };
};
