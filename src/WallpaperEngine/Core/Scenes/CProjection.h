#pragma once

#include "WallpaperEngine/Core/Core.h"

namespace WallpaperEngine::Core::Scenes
{
    using json = nlohmann::json;

    class CProjection
    {
    public:
        static CProjection* fromJSON (json data);

        const uint32_t& getWidth () const;
        const uint32_t& getHeight () const;
    protected:
        CProjection (uint32_t width, uint32_t height);
    private:
        uint32_t m_width;
        uint32_t m_height;
    };
};
