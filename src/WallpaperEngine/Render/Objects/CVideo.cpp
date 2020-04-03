#include "CVideo.h"

using namespace WallpaperEngine;

using namespace WallpaperEngine::Render::Objects;

CVideo::CVideo (CScene* scene, Core::Objects::CVideo* video) :
    Render::CObject (scene, Type, video),
    m_video (video)
{
    m_frameImage = this->getScene ()->getContext ()->getDevice ()->getVideoDriver ()->createImage (irr::video::ECOLOR_FORMAT::ECF_R8G8B8,
                irr::core::dimension2du(m_video->getWidth(), m_video->getHeight()));

    this->m_boundingBox = irr::core::aabbox3d<irr::f32> (0, 0, 0, 0, 0, 0);
}

void CVideo::render ()
{
    irr::video::IVideoDriver* driver = this->getScene ()->getContext ()->getDevice ()->getVideoDriver ();

    m_video->getNextFrame ();
    m_video->writeFrameToImage (m_frameImage);
    
    driver->removeTexture (m_frameTexture);
    m_frameTexture = driver->addTexture ("frameTexture", m_frameImage);

    driver->draw2DImage (m_frameTexture, irr::core::vector2di(0));
}

const irr::core::aabbox3d<irr::f32>& CVideo::getBoundingBox () const
{
    return this->m_boundingBox;
}

const std::string CVideo::Type = "video";