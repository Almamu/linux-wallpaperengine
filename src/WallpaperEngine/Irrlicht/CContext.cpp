#include "CContext.h"

namespace WallpaperEngine::Irrlicht
{
    void CContext::setDevice (irr::IrrlichtDevice* device)
    {
        this->m_device = device;
    }

    irr::IrrlichtDevice* CContext::getDevice ()
    {
        return this->m_device;
    }
};