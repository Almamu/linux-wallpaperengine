#include "CObject.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

CObject::CObject(CScene* scene, std::string type, Core::CObject *object) :
    irr::scene::ISceneNode (
        scene,
        scene->getContext ()->getDevice ()->getSceneManager (),
        object->getId ()
    ),
    m_scene (scene),
    m_object (object),
    m_type (type)
{
}

CObject::~CObject()
{
}

CScene* CObject::getScene () const
{
    return this->m_scene;
}

void CObject::OnRegisterSceneNode ()
{
    if (this->m_object->isVisible () == true)
        SceneManager->registerNodeForRendering (this);

    ISceneNode::OnRegisterSceneNode ();
}