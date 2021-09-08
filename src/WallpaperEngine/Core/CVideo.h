#pragma once

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
                std::string  filename
        );

        const std::string& getFilename ();

    protected:
        friend class CWallpaper;

        const std::string m_filename;

        static const std::string Type;

    private:
    };
};
