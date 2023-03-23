#pragma once

#include <string>
#include <stdexcept>
#include <map>
#include <filesystem>

#include "CContainer.h"
#include "CFileEntry.h"

namespace WallpaperEngine::Assets
{
    /**
     * Directory container implementation, provides access to background files under a specific directory
     */
    class CDirectory : public CContainer
    {
    public:
        explicit CDirectory (std::filesystem::path basepath);
        ~CDirectory ();

        /** @inheritdoc */
        [[nodiscard]] std::filesystem::path resolveRealFile (const std::string& filename) const override;
        /** @inheritdoc */
        [[nodiscard]] const void* readFile (const std::string& filename, uint32_t* length) const override;

    private:
        /** The basepath for the directory */
        std::filesystem::path m_basepath;
        /** File cache to simplify access to data */
        std::map <std::string, CFileEntry> m_cache;
    };
}