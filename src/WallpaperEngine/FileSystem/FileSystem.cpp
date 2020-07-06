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
