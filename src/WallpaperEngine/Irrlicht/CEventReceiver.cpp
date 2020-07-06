#include "CEventReceiver.h"

using namespace WallpaperEngine::Irrlicht;

bool CEventReceiver::OnEvent(const irr::SEvent &event)
{
    if (event.EventType == irr::EET_MOUSE_INPUT_EVENT && event.MouseInput.Event == irr::EMIE_MOUSE_MOVED)
    {
        this->m_position.X = event.MouseInput.X;
        this->m_position.Y = event.MouseInput.Y;
        return true;
    }

    return false;
}

const irr::core::position2di& CEventReceiver::getPosition () const
{
    return this->m_position;
}