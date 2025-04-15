#include "CVideo.h"

#include <utility>

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Core::Wallpapers;

CVideo::CVideo (std::string filename, const CProject& project) :
    CWallpaper (Type, project),
    m_filename (std::move(filename)) {}

const std::string& CVideo::getFilename () const {
    return this->m_filename;
}

const std::string CVideo::Type = "video";
