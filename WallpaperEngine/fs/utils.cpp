#include <cstdint>
#include <sys/stat.h>

// filesystem includes
#include <WallpaperEngine/fs/utils.h>

// engine includes
#include <WallpaperEngine/irrlicht.h>

namespace WallpaperEngine
{
    namespace fs
    {
        std::string utils::loadFullFile (irr::io::path file)
        {
            irr::io::IReadFile* reader = WallpaperEngine::irrlicht::device->getFileSystem ()->createAndOpenFile (file);

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
    }
}