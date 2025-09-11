#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Camera.h"

using namespace WallpaperEngine;
using namespace WallpaperEngine::Render;

Camera::Camera (Wallpapers::CScene& scene, const SceneData::Camera& camera) :
    m_width (0),
    m_height (0),
    m_camera (camera),
    m_scene (scene) {
    // get the lookat position
    // TODO: ENSURE THIS IS ONLY USED WHEN NOT DOING AN ORTOGRAPHIC CAMERA AS IT THROWS OFF POINTS
    this->m_lookat = glm::lookAt (this->getEye (), this->getCenter (), this->getUp ());
}

Camera::~Camera () = default;

const glm::vec3& Camera::getCenter () const {
    return this->m_camera.configuration.center;
}

const glm::vec3& Camera::getEye () const {
    return this->m_camera.configuration.eye;
}

const glm::vec3& Camera::getUp () const {
    return this->m_camera.configuration.up;
}

const glm::mat4& Camera::getProjection () const {
    return this->m_projection;
}

const glm::mat4& Camera::getLookAt () const {
    return this->m_lookat;
}

bool Camera::isOrthogonal () const {
    return this->m_isOrthogonal;
}

Wallpapers::CScene& Camera::getScene () const {
    return this->m_scene;
}

float Camera::getWidth () const {
    return this->m_width;
}

float Camera::getHeight () const {
    return this->m_height;
}

void Camera::setOrthogonalProjection (float width, float height) {
    this->m_width = width;
    this->m_height = height;

    // TODO: GET THE ZNEAR AND ZFAR FROM THE BACKGROUND (IF AVAILABLE)
    // get the orthogonal projection (the float is there to ensure the values are casted to float, so maths do work)
    this->m_projection = glm::ortho<float> (-width / 2.0, width / 2.0, -height / 2.0, height / 2.0, 0, 1000);
    this->m_projection = glm::translate (this->m_projection, this->getEye ());
    // update the orthogonal flag
    this->m_isOrthogonal = true;
}