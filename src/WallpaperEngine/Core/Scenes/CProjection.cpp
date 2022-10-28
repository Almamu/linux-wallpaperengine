#include "CProjection.h"

using namespace WallpaperEngine::Core::Scenes;

CProjection::CProjection (uint32_t width, uint32_t height) :
    m_width (width),
    m_height (height)
{
}

const uint32_t& CProjection::getWidth () const
{
    return this->m_width;
}

const uint32_t& CProjection::getHeight () const
{
    return this->m_height;
}

CProjection* CProjection::fromJSON (json data)
{
    auto auto_it = jsonFindDefault <bool> (data, "auto", false);

    auto width_it = jsonFindRequired (data, "width", "Projection must have width");
    auto height_it = jsonFindRequired (data, "height", "Projection must have height");

    // TODO: PROPERLY SUPPORT AUTO-DETECTING SIZE
    if (auto_it == true)
        return new CProjection (1920, 1080);
    else
        return new CProjection (
            *width_it,
            *height_it
        );
}