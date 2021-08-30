// filesystem includes
#include "FileSystem.h"

#include <iostream>

// engine includes
#include "WallpaperEngine/Irrlicht/CContext.h"

extern WallpaperEngine::Irrlicht::CContext* IrrlichtContext;

using namespace WallpaperEngine;

std::string FileSystem::loadFullFile (const irr::io::path& file)
{
    std::cout << file.c_str() << std::endl;
    irr::io::IReadFile* reader = IrrlichtContext->getDevice ()->getFileSystem ()->createAndOpenFile (file);

    if (reader == nullptr)
        throw std::runtime_error ("Cannot open file " + std::string (file.c_str ()) + " for reading");

    char* filedata = new char [reader->getSize () + 1];
    memset (filedata, 0, reader->getSize () + 1);

    reader->read (filedata, reader->getSize ());
    reader->drop ();

    std::string content = filedata;
    delete [] filedata;

    return content;
}

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