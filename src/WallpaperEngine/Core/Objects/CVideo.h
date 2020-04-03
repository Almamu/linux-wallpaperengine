#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/CObject.h"

extern "C"
{
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

namespace WallpaperEngine::Core::Objects
{
    class CVideo : public CObject
    {
        friend class CObject;

    public:
        CVideo (
                const irr::io::path& filename
        );

        void initFrames (int width, int height);

        void getNextFrame ();
        void writeFrameToImage (irr::video::IImage* image);

        int getWidth () const;
        int getHeight () const;

    protected:
        void restartStream ();

        static const std::string Type;

    private:
        AVFormatContext* m_formatCtx = nullptr;
        AVCodecContext* m_codecCtx = nullptr;
        AVFrame* m_videoFrame = nullptr;
        AVFrame* m_videoFrameRGB = nullptr;
        SwsContext* m_swsCtx = nullptr;

        int m_videoStream = -1, m_audioStream = -1;
        int m_width, m_height;
    };
};
