#include "CProjection.h"

using namespace WallpaperEngine::Core::Scenes;

CProjection::CProjection (int width, int height) : m_isAuto (false), m_width (width), m_height (height) {}

CProjection::CProjection (bool isAuto) : m_isAuto (isAuto), m_width (0), m_height (0) {}

const int& CProjection::getWidth () const {
    return this->m_width;
}

const int& CProjection::getHeight () const {
    return this->m_height;
}

bool CProjection::isAuto () const {
    return this->m_isAuto;
}

void CProjection::setWidth (int width) {
    this->m_width = width;
}

void CProjection::setHeight (int height) {
    this->m_height = height;
}

CProjection* CProjection::fromJSON (json data) {
    const auto auto_it = jsonFindDefault<bool> (data, "auto", false);

    const auto width_it = jsonFindRequired (data, "width", "Projection must have width");
    const auto height_it = jsonFindRequired (data, "height", "Projection must have height");

    // TODO: PROPERLY SUPPORT AUTO-DETECTING SIZE
    if (auto_it)
        return new CProjection (true);

    return new CProjection (*width_it, *height_it);
}