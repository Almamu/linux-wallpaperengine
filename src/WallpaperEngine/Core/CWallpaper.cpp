#include "CWallpaper.h"

#include <utility>

using namespace WallpaperEngine::Core;

CWallpaper::CWallpaper (const CProject& project) :
    m_project (project) {}

const CProject& CWallpaper::getProject () const {
    return this->m_project;
}
