#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/CVideo.h"

#include "WallpaperEngine/Render/CWallpaper.h"

namespace WallpaperEngine::Render
{
    class CWallpaper;

    class CVideo : public CWallpaper
    {
    public:
        CVideo (Core::CVideo* video, irr::video::IVideoDriver* driver);

        void renderWallpaper () override;

    protected:
        friend class CWallpaper;

        static const std::string Type;

    private:
        irr::video::IImage* m_frameImage;
        irr::video::ITexture* m_frameTexture;

        irr::video::IVideoDriver* m_driver;
    };
};
