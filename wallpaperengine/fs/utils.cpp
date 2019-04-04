#include <cstdint>
#include <sys/stat.h>

// filesystem includes
#include <wallpaperengine/fs/utils.h>

// engine includes
#include <wallpaperengine/irrlicht.h>

namespace wp
{
    namespace fs
    {
        std::string utils::loadFullFile (irr::io::path file)
        {
            irr::io::IReadFile* reader = wp::irrlicht::device->getFileSystem ()->createAndOpenFile (file);

            if (reader == NULL)
                return "";

            char* filedata = new char [reader->getSize () + 1];
            memset (filedata, 0, reader->getSize () + 1);

            reader->read (filedata, reader->getSize ());

            std::string content = filedata;
            delete [] filedata;

            return content;
        }
    }
}