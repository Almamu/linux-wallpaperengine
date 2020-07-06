#include "CWallpaper.h"

using namespace WallpaperEngine::Core;

CWallpaper::CWallpaper (std::string type) :
    m_type (type)
{
}

CProject* CWallpaper::getProject ()
{
    return this->m_project;
}

void CWallpaper::setProject (CProject* project)
{
    this->m_project = project;
}
