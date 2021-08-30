#include "CCamera.h"

using namespace WallpaperEngine::Core::Scenes;
using namespace WallpaperEngine::Core::Types;

CCamera::CCamera (glm::vec3 center, glm::vec3 eye, glm::vec3 up) :
    m_center (center),
    m_eye (eye),
    m_up (up)
{
}

const glm::vec3& CCamera::getCenter () const
{
    return this->m_center;
}

const glm::vec3& CCamera::getEye () const
{
    return this->m_eye;
}

const glm::vec3& CCamera::getUp () const
{
    return this->m_up;
}

CCamera* CCamera::fromJSON (json data)
{
    auto center_it = jsonFindRequired (data, "center", "Camera must have a center position");
    auto eye_it = jsonFindRequired (data, "eye", "Camera must have an eye position");
    auto up_it = jsonFindRequired (data, "up", "Camera must have a up position");

    return new CCamera (
        WallpaperEngine::Core::aToVector3 (*center_it),
        WallpaperEngine::Core::aToVector3 (*eye_it),
        WallpaperEngine::Core::aToVector3 (*up_it)
    );
}