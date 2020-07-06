#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Scenes
{
    using json = nlohmann::json;

    class CProjection
    {
    public:
        static CProjection* fromJSON (json data);

        const irr::u32& getWidth () const;
        const irr::u32& getHeight () const;
    protected:
        CProjection (irr::u32 width, irr::u32 height);
    private:
        irr::u32 m_width;
        irr::u32 m_height;
    };
};
