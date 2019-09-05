// filesystem includes
#include "FileSystem.h"

// engine includes
#include "WallpaperEngine/Irrlicht/CContext.h"

extern WallpaperEngine::Irrlicht::CContext* IrrlichtContext;

namespace WallpaperEngine::FileSystem
{
    std::string loadFullFile (irr::io::path file)
    {
        irr::io::IReadFile* reader = IrrlichtContext->getDevice ()->getFileSystem ()->createAndOpenFile (file);

        if (reader == NULL)
            throw std::runtime_error ("Cannot open file " + std::string (file.c_str ()) + " for reading");

        char* filedata = new char [reader->getSize () + 1];
        memset (filedata, 0, reader->getSize () + 1);

        reader->read (filedata, reader->getSize ());
        reader->drop ();

        std::string content = filedata;
        delete [] filedata;

        return content;
    }
};