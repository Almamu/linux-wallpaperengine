#pragma once

#include <string>
#include <map>

#include "CFileEntry.h"
#include "CContainer.h"

namespace WallpaperEngine::Assets
{
    class CVirtualContainer : public CContainer
    {
    public:
        CVirtualContainer () {}
        ~CVirtualContainer () {}

        /**
         * Adds a new file to the virtual container
         *
         * @param filename
         * @param contents
         * @param length
         */
        void add (const std::string& filename, void* contents, uint32_t length);

        /**
         * Adds a new file to the virtual container
         *
         * @param filename
         * @param contents
         */
        void add (const std::string& filename, const std::string& contents);

        const void* readFile (std::string filename, uint32_t* length) const override;

    private:
        std::map <std::string, CFileEntry> m_virtualFiles;
    };
}