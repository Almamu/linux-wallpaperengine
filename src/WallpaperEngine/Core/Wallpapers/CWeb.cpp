#include "CWeb.h"

#include "common.h"
#include <utility>

using namespace WallpaperEngine::Core;

const std::string& CWeb::getFilename () {
    return this->m_filename;
}

CWeb::CWeb (std::string filename, CProject& project) : CWallpaper (Type, project), m_filename (std::move (filename)) {}

const std::string CWeb::Type = "web";
