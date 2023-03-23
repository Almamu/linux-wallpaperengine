#pragma once

#include <string>
#include <map>

#include "CFileEntry.h"
#include "CContainer.h"

namespace WallpaperEngine::Assets
{
    /**
     * Virtual container implementation, provides virtual files for the backgrounds to use
     */
    class CVirtualContainer : public CContainer
    {
    public:
        CVirtualContainer () = default;
        ~CVirtualContainer () = default;

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
        /** @inheritdoc */
        const void* readFile (const std::string& filename, uint32_t* length) const override;

    private:
        /** The recorded files in this virtual container */
        std::map <std::string, CFileEntry> m_virtualFiles;
    };
}