#include "CVideo.h"

using namespace WallpaperEngine::Core;

CVideo::CVideo (
        const irr::io::path& filename) :
        CWallpaper (Type),
        m_filename (filename)
{
}

const irr::io::path CVideo::getFilename ()
{
    return this->m_filename;
}

const std::string CVideo::Type = "video";
