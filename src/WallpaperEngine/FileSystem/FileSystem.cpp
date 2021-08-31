// filesystem includes
#include "FileSystem.h"

#include <iostream>

using namespace WallpaperEngine;

std::string FileSystem::loadFullFile (const std::string& file, WallpaperEngine::Assets::CContainer* containers)
{
    uint32_t length = 0;
    void* contents = containers->readFile (file, &length);

    // build a new buffer that can fit in the string
    char* filedata = new char [length + 1];
    // ensure there's a null termination at the end
    memset (filedata, 0, length + 1);
    // copy file contents over
    memcpy (filedata, contents, length);

    std::string content = filedata;

    delete [] filedata;

    return content;
}