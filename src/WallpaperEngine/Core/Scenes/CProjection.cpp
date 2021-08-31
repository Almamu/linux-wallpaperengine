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
    auto width_it = jsonFindRequired (data, "width", "Projection must have width");
    auto height_it = jsonFindRequired (data, "height", "Projection must have height");

    return new CProjection (
        *width_it,
        *height_it
    );
}