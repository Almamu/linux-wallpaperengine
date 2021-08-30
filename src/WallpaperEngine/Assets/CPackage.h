//
// Created by almamu on 8/8/21.
//

#pragma once

#include <string>
#include <vector>
#include <map>
#include <exception>
#include <stdexcept>
#include <cstring>

#include "CContainer.h"
#include "CFileEntry.h"

namespace WallpaperEngine::Assets
{
    class CPackage : public CContainer
    {
    public:
        CPackage (const std::string& path);
        ~CPackage ();

        void* readFile (std::string filename, uint32_t* length) override;

    protected:
        /**
         * Loads the current package file and loads all it's contents to memory
         */
        void init ();

        /**
         * Reads the header from the current position and ensures it's a compatible version
         *
         * @param fp The file where to read from
         */
        void validateHeader (FILE* fp);

        /**
         * Loads the files in the package into memory
         *
         * @param fp The file where to read from
         */
        void loadFiles (FILE* fp);

        /**
         * Reads a size-prefixed string
         *
         * @param fp File to read from
         *
         * @return The read data, important to free it
         */
        char* readSizedString (FILE* fp);
        /**
         * Reads a simple unsigned of 32 bits
         *
         * @param fp File to read from
         *
         * @return The read value
         */
        uint32_t readInteger (FILE* fp);

    private:
        std::string m_path;
        std::map <std::string, CFileEntry> m_contents;
    };
}
