#include "CAssetLoadException.h"

using namespace WallpaperEngine::Assets;

CAssetLoadException::CAssetLoadException(const std::string& filename, const std::string& extrainfo)
    : m_message("Cannot find file " + filename + ": " + extrainfo)
{

}

const char *CAssetLoadException::what() const noexcept
{
    return this->m_message.c_str ();
}
