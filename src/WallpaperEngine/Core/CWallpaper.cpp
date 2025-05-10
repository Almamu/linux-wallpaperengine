#include "CWallpaper.h"

#include <utility>

using namespace WallpaperEngine::Core;

CWallpaper::CWallpaper (std::shared_ptr <const CProject> project) :
    m_project (project) {}

std::shared_ptr <const CProject> CWallpaper::getProject () const {
    return this->m_project;
}
