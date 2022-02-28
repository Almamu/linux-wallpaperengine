#include "CPackageLoadException.h"

using namespace WallpaperEngine::Assets;

CPackageLoadException::CPackageLoadException(const std::string& filename, const std::string& extrainfo)
        : m_message("Cannot load package " + filename + ": " + extrainfo)
{
}

const char *CPackageLoadException::what() const noexcept
{
    return this->m_message.c_str ();
}
