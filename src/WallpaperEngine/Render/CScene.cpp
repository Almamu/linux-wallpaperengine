#include "WallpaperEngine/Irrlicht/CContext.h"

#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/CSound.h"

#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/CSound.h"

#include "CScene.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

CScene::CScene (const Core::CProject* project, Irrlicht::CContext* context) :
    irr::scene::ISceneNode (
        context->getDevice ()->getSceneManager ()->getRootSceneNode (),
        context->getDevice ()->getSceneManager ()
    ),
    m_project (project),
    m_scene (project->getScene ()),
    m_context (context)
{
    this->m_camera = new CCamera (this, this->m_project->getScene ()->getCamera ());
    this->m_camera->setOrthogonalProjection (
            this->m_scene->getOrthogonalProjection ()->getWidth (),
            this->m_scene->getOrthogonalProjection ()->getHeight ()
    );

    auto cur = this->m_scene->getObjects ().begin ();
    auto end = this->m_scene->getObjects ().end ();

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

const Core::CScene* CScene::getScene () const
{
    return this->m_scene;
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

const irr::core::aabbox3d<irr::f32>& CScene::getBoundingBox () const
{
    return this->m_boundingBox;
}
void CScene::OnRegisterSceneNode ()
{
    SceneManager->registerNodeForRendering (this);

    ISceneNode::OnRegisterSceneNode ();
}