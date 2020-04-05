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

        void initFrames (int width, int height);

        void getNextFrame ();
        void writeFrameToImage (irr::video::IImage* image);

    protected:
        friend class CWallpaper;

        static const std::string Type;

        void restartStream ();

    private:
        AVFormatContext* m_formatCtx = nullptr;
        AVCodecContext* m_codecCtx = nullptr;
        AVFrame* m_videoFrame = nullptr;
        AVFrame* m_videoFrameRGB = nullptr;
        SwsContext* m_swsCtx = nullptr;

        int m_videoStream = -1, m_audioStream = -1;
    };
};
