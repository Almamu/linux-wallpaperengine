#include "CCamera.h"
#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Scenes;

CCamera::CCamera (irr::core::vector3df center, irr::core::vector3df eye, irr::core::vector3df up) :
    m_center (center),
    m_eye (eye),
    m_up (up)
{
}

irr::core::vector3df* CCamera::getCenter ()
{
    return &this->m_center;
}

irr::core::vector3df* CCamera::getEye ()
{
    return &this->m_eye;
}

irr::core::vector3df* CCamera::getUp ()
{
    return &this->m_up;
}

CCamera* CCamera::fromJSON (json data)
{
    json::const_iterator center_it = data.find ("center");
    json::const_iterator eye_it = data.find ("eye");
    json::const_iterator up_it = data.find ("up");

    if (center_it == data.end ())
    {
        throw std::runtime_error ("Camera must have a center position");
    }

    if (eye_it == data.end ())
    {
        throw std::runtime_error ("Camera must have an eye position");
    }

    if (up_it == data.end ())
    {
        throw std::runtime_error ("Camera must have a up position");
    }

    return new CCamera (
            WallpaperEngine::Core::ato3vf (*center_it),
            WallpaperEngine::Core::ato3vf (*eye_it),
            WallpaperEngine::Core::ato3vf (*up_it)
    );
}