#include "projection.h"
#include "../../core.h"

using namespace wp::core::scenes;

projection::projection (irr::u32 width, irr::u32 height) :
    m_width (width),
    m_height (height)
{
}

irr::u32 projection::getWidth ()
{
    return this->m_width;
}

irr::u32 projection::getHeight ()
{
    return this->m_height;
}

projection* projection::fromJSON (json data)
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

    return new projection (
        *width_it,
        *height_it
    );
}