#include "AssetLoadException.h"

using namespace WallpaperEngine::Render;

AssetLoadException::AssetLoadException (const std::string& filename, const std::string& extrainfo) :
    m_message ("Cannot find file " + filename + ": " + extrainfo) {}

const char* AssetLoadException::what () const noexcept {
    return this->m_message.c_str ();
}
