#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

namespace wp::core::scenes
{
    using json = nlohmann::json;

    class projection
    {
    public:
        static projection* fromJSON (json data);

        irr::u32 getWidth ();
        irr::u32 getHeight ();
    protected:
        projection (irr::u32 width, irr::u32 height);
    private:
        irr::u32 m_width;
        irr::u32 m_height;
    };
};
