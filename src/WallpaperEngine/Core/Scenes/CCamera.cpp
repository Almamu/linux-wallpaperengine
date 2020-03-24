#include "CCamera.h"
#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Scenes;

CCamera::CCamera (irr::core::vector3df center, irr::core::vector3df eye, irr::core::vector3df up) :
    m_center (center),
    m_eye (eye),
    m_up (up)
{
}

const irr::core::vector3df& CCamera::getCenter () const
{
    return this->m_center;
}

const irr::core::vector3df& CCamera::getEye () const
{
    return this->m_eye;
}

const irr::core::vector3df& CCamera::getUp () const
{
    return this->m_up;
}

CCamera* CCamera::fromJSON (json data)
{
    auto center_it = jsonFindRequired (&data, "center", "Camera must have a center position");
    auto eye_it = jsonFindRequired (&data, "eye", "Camera must have an eye position");
    auto up_it = jsonFindRequired (&data, "up", "Camera must have a up position");

    return new CCamera (
        WallpaperEngine::Core::ato3vf (*center_it),
        WallpaperEngine::Core::ato3vf (*eye_it),
        WallpaperEngine::Core::ato3vf (*up_it)
    );
}