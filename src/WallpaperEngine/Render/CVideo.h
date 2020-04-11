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
        CVideo (Core::CVideo* video, WallpaperEngine::Irrlicht::CContext* context);

        void render () override;

        Core::CVideo* getVideo ();

    protected:
        friend class CWallpaper;

        static const std::string Type;

    private:
        void getNextFrame ();
        void writeFrameToImage ();

        irr::video::IImage* m_frameImage;
        irr::video::ITexture* m_frameTexture;
    };
};
