#include "CWeb.h"

#include <utility>

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Wallpapers;

const std::string& CWeb::getFilename () {
    return this->m_filename;
}

CWeb::CWeb (std::string filename, CProject& project) : CWallpaper (Type, project), m_filename (std::move (filename)) {}

const std::string CWeb::Type = "web";
