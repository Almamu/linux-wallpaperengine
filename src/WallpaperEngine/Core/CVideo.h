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

        const irr::io::path getFilename ();

    protected:
        friend class CWallpaper;

        const irr::io::path m_filename;

        static const std::string Type;

    private:
    };
};
