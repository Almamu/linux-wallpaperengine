#include "CVideo.h"

using namespace WallpaperEngine;

using namespace WallpaperEngine::Render;

CVideo::CVideo (Core::CVideo* video, irr::video::IVideoDriver* driver) :
    CWallpaper (video, Type),
    m_driver (driver)
{
    int width = driver->getScreenSize ().Width;
    int height = driver->getScreenSize ().Height;
    video->initFrames (width, height);
    m_frameImage = m_driver->createImage (irr::video::ECOLOR_FORMAT::ECF_R8G8B8,
                irr::core::dimension2du(width, height));
}

void CVideo::renderWallpaper ()
{
    Core::CVideo* video = m_wallpaperData->as <Core::CVideo> ();
    video->getNextFrame ();
    video->writeFrameToImage (m_frameImage);

    m_driver->removeTexture (m_frameTexture);
    m_frameTexture = m_driver->addTexture ("frameTexture", m_frameImage);

    m_driver->draw2DImage (m_frameTexture, irr::core::vector2di(0));
}

const std::string CVideo::Type = "video";
