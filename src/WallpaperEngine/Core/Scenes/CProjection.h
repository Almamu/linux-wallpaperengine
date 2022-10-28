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
        const bool isAuto () const;

        void setWidth (uint32_t width);
        void setHeight (uint32_t height);

    protected:
        CProjection (uint32_t width, uint32_t height);
        CProjection (bool isAuto);
    private:
        uint32_t m_width;
        uint32_t m_height;
        bool m_isAuto;
    };
};
