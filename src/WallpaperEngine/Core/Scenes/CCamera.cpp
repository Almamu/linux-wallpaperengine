#include "CCamera.h"

using namespace WallpaperEngine::Core::Scenes;

CCamera::CCamera (glm::vec3 center, glm::vec3 eye, glm::vec3 up) :
    m_center (center),
    m_eye (eye),
    m_up (up) {}

const glm::vec3& CCamera::getCenter () const {
    return this->m_center;
}

const glm::vec3& CCamera::getEye () const {
    return this->m_eye;
}

const glm::vec3& CCamera::getUp () const {
    return this->m_up;
}

const CCamera* CCamera::fromJSON (const json::const_iterator& data) {
    return new CCamera (
        jsonFindRequired <glm::vec3> (data, "center", "Camera must have a center position"),
        jsonFindRequired <glm::vec3> (data, "eye", "Camera must have an eye position"),
        jsonFindRequired <glm::vec3> (data, "up", "Camera must have a up position")
    );
}