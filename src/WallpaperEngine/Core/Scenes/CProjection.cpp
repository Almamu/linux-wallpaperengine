#include "CProjection.h"
#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Scenes;

CProjection::CProjection (irr::u32 width, irr::u32 height) :
    m_width (width),
    m_height (height)
{
}

const irr::u32& CProjection::getWidth () const
{
    return this->m_width;
}

const irr::u32& CProjection::getHeight () const
{
    return this->m_height;
}

CProjection* CProjection::fromJSON (json data)
{
    auto width_it = jsonFindRequired (&data, "width", "Projection must have width");
    auto height_it = jsonFindRequired (&data, "height", "Projection must have height");

    return new CProjection (
        *width_it,
        *height_it
    );
}