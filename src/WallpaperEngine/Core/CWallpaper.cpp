#include "CWallpaper.h"

#include <utility>

using namespace WallpaperEngine::Core;

CWallpaper::CWallpaper (std::string type, const CProject& project) :
    m_project (project),
    m_type (std::move(type)) {}

const CProject& CWallpaper::getProject () const {
    return this->m_project;
}
