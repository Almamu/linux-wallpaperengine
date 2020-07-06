#include "WallpaperEngine/Core/Objects/CImage.h"
#include "WallpaperEngine/Core/Objects/CSound.h"
#include "WallpaperEngine/Core/Objects/CParticle.h"

#include "WallpaperEngine/Render/Objects/CImage.h"
#include "WallpaperEngine/Render/Objects/CSound.h"
#include "CScene.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

CScene::CScene (Core::CProject* project, Irrlicht::CContext* context) :
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

    std::vector<Core::CObject*>::const_iterator cur = this->m_scene->getObjects ()->begin ();
    std::vector<Core::CObject*>::const_iterator end = this->m_scene->getObjects ()->end ();

    int highestId = 0;

    for (; cur != end; cur ++)
    {
        if ((*cur)->getId () > highestId)
            highestId = (*cur)->getId ();

        if ((*cur)->Is <Core::Objects::CImage> () == true)
        {
            new Objects::CImage (this, (*cur)->As <Core::Objects::CImage> ());
        }
        else if ((*cur)->Is <Core::Objects::CSound> () == true)
        {
            new Objects::CSound (this, (*cur)->As <Core::Objects::CSound> ());
        }
        else if ((*cur)->Is <Core::Objects::CParticle> () == true)
        {
            this->getContext ()->getDevice ()->getLogger ()->log ("Particles disabled, not supported yet", irr::ELL_ERROR);
        }
        else
        {
            throw std::runtime_error ("unsupported object type found");
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

Core::CScene* CScene::getScene ()
{
    return this->m_scene;
}

CCamera* CScene::getCamera ()
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

const irr::core::aabbox3d<irr::f32>& CScene::getBoundingBox() const
{
    return this->m_boundingBox;
}
void CScene::OnRegisterSceneNode ()
{
    SceneManager->registerNodeForRendering (this);

    ISceneNode::OnRegisterSceneNode ();
}