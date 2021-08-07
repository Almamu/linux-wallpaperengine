#include "CCamera.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

CCamera::CCamera (CScene* scene, const Core::Scenes::CCamera* camera) :
    m_camera (camera),
    m_scene (scene)
{
    this->m_sceneCamera = scene->getContext ()->getDevice ()->getSceneManager ()->addCameraSceneNode (
        scene, this->getEye (), this->getCenter (), scene->nextId ()
    );
    this->m_sceneCamera->setUpVector (this->getUp ());
}

CCamera::~CCamera ()
{
    this->m_sceneCamera->remove ();
}

const irr::core::vector3df& CCamera::getCenter () const
{
    return this->m_camera->getCenter ();
}

const irr::core::vector3df& CCamera::getEye () const
{
    return this->m_camera->getEye ();
}

const irr::core::vector3df& CCamera::getUp () const
{
    return this->m_camera->getUp ();
}

void CCamera::setOrthogonalProjection (irr::f32 width, irr::f32 height)
{
    irr::core::matrix4 identity; identity.makeIdentity ();
    irr::core::matrix4 orthogonalProjection; orthogonalProjection.buildProjectionMatrixOrthoLH (
        width, height,
        this->getCenter ().Z,
        this->getEye ().Z
    );

    this->m_sceneCamera->setProjectionMatrix (orthogonalProjection, true);
    this->m_scene->getContext ()->getDevice ()->getVideoDriver ()->setTransform (irr::video::ETS_PROJECTION, orthogonalProjection);
    this->m_scene->getContext ()->getDevice ()->getVideoDriver ()->setTransform (irr::video::ETS_VIEW, identity);
    this->m_scene->getContext ()->getDevice ()->getVideoDriver ()->setTransform (irr::video::ETS_WORLD, identity);
}