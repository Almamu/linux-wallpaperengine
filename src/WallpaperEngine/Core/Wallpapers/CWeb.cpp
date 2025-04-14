#include "CWeb.h"

#include <utility>

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Wallpapers;

const std::string& CWeb::getFilename () const {
    return this->m_filename;
}

CWeb::CWeb (std::string filename, const CProject& project) :
    CWallpaper (Type, project),
    m_filename (filename) {}

const std::string CWeb::Type = "web";
