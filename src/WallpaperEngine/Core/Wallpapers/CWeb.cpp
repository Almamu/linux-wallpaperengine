#include "CWeb.h"

#include <utility>

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Wallpapers;

const std::string& CWeb::getFilename () const {
    return this->m_filename;
}

CWeb::CWeb (std::string filename, std::shared_ptr <const CProject> project) :
    CWallpaper (project),
    m_filename (std::move(filename)) {}
