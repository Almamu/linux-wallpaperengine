#include "CProjection.h"

using namespace WallpaperEngine::Core::Scenes;

CProjection::CProjection (uint32_t width, uint32_t height) :
    m_width (width),
    m_height (height)
{
}

CProjection::CProjection (bool isAuto) :
    m_isAuto (isAuto)
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

const bool CProjection::isAuto () const
{
    return this->m_isAuto;
}

void CProjection::setWidth (uint32_t width)
{
    this->m_width = width;
}

void CProjection::setHeight (uint32_t height)
{
    this->m_height = height;
}

CProjection* CProjection::fromJSON (json data)
{
    auto auto_it = jsonFindDefault <bool> (data, "auto", false);

    auto width_it = jsonFindRequired (data, "width", "Projection must have width");
    auto height_it = jsonFindRequired (data, "height", "Projection must have height");

    // TODO: PROPERLY SUPPORT AUTO-DETECTING SIZE
    if (auto_it == true)
        return new CProjection (true);
    else
        return new CProjection (
            *width_it,
            *height_it
        );
}