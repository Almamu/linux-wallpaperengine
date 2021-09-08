#include "CVideo.h"

#include <utility>

using namespace WallpaperEngine::Core;

CVideo::CVideo (
        std::string  filename) :
        CWallpaper (Type),
        m_filename (std::move(filename))
{
}

const std::string& CVideo::getFilename ()
{
    return this->m_filename;
}

const std::string CVideo::Type = "video";
