#include "CWallpaper.h"

using namespace WallpaperEngine::Render;

CWallpaper::CWallpaper (Core::CWallpaper* wallpaperData, std::string type, WallpaperEngine::Irrlicht::CContext* context) :
    irr::scene::ISceneNode (
        context->getDevice ()->getSceneManager ()->getRootSceneNode (),
        context->getDevice ()->getSceneManager ()
    ),
    m_context (context),
    m_wallpaperData (wallpaperData),
    m_type (type)
{
}

CWallpaper::~CWallpaper ()
{
}

void CWallpaper::OnRegisterSceneNode ()
{
    SceneManager->registerNodeForRendering (this);

    ISceneNode::OnRegisterSceneNode ();
}

WallpaperEngine::Irrlicht::CContext* CWallpaper::getContext () const
{
    return this->m_context;
}

const irr::core::aabbox3d<irr::f32>& CWallpaper::getBoundingBox () const
{
    return this->m_boundingBox;
}

WallpaperEngine::Core::CWallpaper* CWallpaper::getWallpaperData ()
{
    return this->m_wallpaperData;
}
