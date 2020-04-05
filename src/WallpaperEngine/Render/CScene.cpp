#include "WallpaperEngine/Irrlicht/CContext.h"

#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/CSound.h"

#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/CSound.h"

#include "CScene.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

CScene::CScene (Core::CScene* scene, Irrlicht::CContext* context) :
    CWallpaper (scene, Type),
    irr::scene::ISceneNode (
        context->getDevice ()->getSceneManager ()->getRootSceneNode (),
        context->getDevice ()->getSceneManager ()
    ),
    m_context (context)
{
    this->m_camera = new CCamera (this, scene->getCamera ());
    this->m_camera->setOrthogonalProjection (
            scene->getOrthogonalProjection ()->getWidth (),
            scene->getOrthogonalProjection ()->getHeight ()
    );

    auto cur = scene->getObjects ().begin ();
    auto end = scene->getObjects ().end ();

    int highestId = 0;

    for (; cur != end; cur ++)
    {
        if ((*cur)->getId () > highestId)
            highestId = (*cur)->getId ();

        if ((*cur)->is<Core::Objects::CImage>() == true)
        {
            new Objects::CImage (this, (*cur)->as<Core::Objects::CImage>());
        }
        else if ((*cur)->is<Core::Objects::CSound>() == true)
        {
            new Objects::CSound (this, (*cur)->as<Core::Objects::CSound>());
        }
    }

    this->m_nextId = ++highestId;
    this->setAutomaticCulling (irr::scene::EAC_OFF);
    this->m_boundingBox = irr::core::aabbox3d<irr::f32>(0, 0, 0, 0, 0, 0);
}

CScene::~CScene ()
{
}

Irrlicht::CContext* CScene::getContext ()
{
    return this->m_context;
}

CCamera* CScene::getCamera () const
{
    return this->m_camera;
}

int CScene::nextId ()
{
    return this->m_nextId ++;
}

void CScene::render ()
{
}

void CScene::renderWallpaper ()
{
    this->m_context->renderFrame (this);
}

const irr::core::aabbox3d<irr::f32>& CScene::getBoundingBox () const
{
    return this->m_boundingBox;
}
void CScene::OnRegisterSceneNode ()
{
    SceneManager->registerNodeForRendering (this);

    ISceneNode::OnRegisterSceneNode ();
}

const std::string CScene::Type = "scene";
