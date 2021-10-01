#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "CCamera.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;
using namespace WallpaperEngine::Core::Types;

CCamera::CCamera (CScene* scene, const Core::Scenes::CCamera* camera) :
    m_camera (camera),
    m_scene (scene),
    m_isOrthogonal (false)
{
    // get the lookat position
    // TODO: ENSURE THIS IS ONLY USED WHEN NOT DOING AN ORTOGRAPHIC CAMERA AS IT THROWS OFF POINTS
    this->m_lookat = glm::lookAt (this->getEye (), this->getCenter (), this->getUp ());
}

CCamera::~CCamera ()
{

}

const glm::vec3& CCamera::getCenter () const
{
    return this->m_camera->getCenter ();
}

const glm::vec3& CCamera::getEye () const
{
    return this->m_camera->getEye ();
}

const glm::vec3& CCamera::getUp () const
{
    return this->m_camera->getUp ();
}

const glm::mat4& CCamera::getProjection () const
{
    return this->m_projection;
}

const glm::mat4& CCamera::getLookAt () const
{
    return this->m_lookat;
}

const bool CCamera::isOrthogonal () const
{
    return this->m_isOrthogonal;
}

void CCamera::setOrthogonalProjection (float width, float height)
{
    // TODO: GET THE ZNEAR AND ZFAR FROM THE BACKGROUND (IF AVAILABLE)
    // get the orthogonal projection (the float is there to ensure the values are casted to float, so maths do work)
    this->m_projection = glm::ortho <float> (-width / 2, width / 2, -height / 2, height / 2, 0, 1000);
    this->m_projection = glm::translate (this->m_projection, this->getEye ());
    // update the orthogonal flag
    this->m_isOrthogonal = true;
}