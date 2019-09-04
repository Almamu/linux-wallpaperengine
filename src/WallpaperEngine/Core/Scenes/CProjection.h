#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Core::Scenes
{
    using json = nlohmann::json;

    class CProjection
    {
    public:
        static CProjection* fromJSON (json data);

        irr::u32 getWidth ();
        irr::u32 getHeight ();
    protected:
        CProjection (irr::u32 width, irr::u32 height);
    private:
        irr::u32 m_width;
        irr::u32 m_height;
    };
};
