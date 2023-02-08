#include "CWallpaper.h"

#include <utility>

using namespace WallpaperEngine::Core;

CWallpaper::CWallpaper (std::string  type) :
    m_type (std::move(type)),
	m_project (nullptr)
{
}

CProject* CWallpaper::getProject () const
{
    return this->m_project;
}

void CWallpaper::setProject (CProject* project)
{
    this->m_project = project;
}
