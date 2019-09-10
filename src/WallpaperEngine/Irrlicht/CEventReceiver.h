#pragma once

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Irrlicht
{
    class CEventReceiver : public irr::IEventReceiver
    {
    public:
        // This is the one method that we have to implement
        bool OnEvent(const irr::SEvent& event) override;

        const irr::core::position2di& getPosition () const;

    private:
        irr::core::position2di m_position;
    };
}
