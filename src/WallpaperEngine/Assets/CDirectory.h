//
// Created by almamu on 8/8/21.
//
#pragma once

#include <string>
#include <stdexcept>
#include <map>

#include "CContainer.h"
#include "CFileEntry.h"

namespace WallpaperEngine::Assets
{
    class CDirectory : public CContainer
    {
    public:
        CDirectory (std::string basepath);
        ~CDirectory ();

        void* readFile (std::string filename, uint32_t* length) override;
    private:
        std::string m_basepath;
        std::map <std::string, CFileEntry> m_cache;
    };
};