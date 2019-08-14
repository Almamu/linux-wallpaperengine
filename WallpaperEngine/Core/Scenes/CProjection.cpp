#include "CProjection.h"
#include "WallpaperEngine/Core/Core.h"

using namespace WallpaperEngine::Core::Scenes;

CProjection::CProjection (irr::u32 width, irr::u32 height) :
    m_width (width),
    m_height (height)
{
}

irr::u32 CProjection::getWidth ()
{
    return this->m_width;
}

irr::u32 CProjection::getHeight ()
{
    return this->m_height;
}

CProjection* CProjection::fromJSON (json data)
{
    json::const_iterator width_it = data.find ("width");
    json::const_iterator height_it = data.find ("height");

    if (width_it == data.end ())
    {
        throw std::runtime_error ("Projection must have width");
    }

    if (height_it == data.end ())
    {
        throw std::runtime_error ("Projection must have height");
    }

    return new CProjection (
        *width_it,
        *height_it
    );
}