#include "CWallpaper.h"

#include <utility>

using namespace WallpaperEngine::Core;

CWallpaper::CWallpaper (std::string type, CProject& project) : m_project (project), m_type (std::move (type)) {}

CProject& CWallpaper::getProject () const {
    return this->m_project;
}
