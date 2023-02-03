#pragma once

#include <string>
#include <stdexcept>
#include <map>
#include <filesystem>

#include "CContainer.h"
#include "CFileEntry.h"

namespace WallpaperEngine::Assets
{
    class CDirectory : public CContainer
    {
    public:
        CDirectory (std::filesystem::path  basepath);
        ~CDirectory ();

        const void* readFile (std::string filename, uint32_t* length) const override;
    private:
        std::filesystem::path m_basepath;
        std::map <std::string, CFileEntry> m_cache;
    };
};