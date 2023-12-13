#include "CWallpaper.h"

#include <utility>

using namespace WallpaperEngine::Core;

CWallpaper::CWallpaper (std::string type, CProject& project) : m_type (std::move (type)), m_project (project) {}

CProject& CWallpaper::getProject () const {
    return this->m_project;
}
