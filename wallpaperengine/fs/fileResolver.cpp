#include <cstdint>
#include <sys/stat.h>

// filesystem includes
#include <wallpaperengine/fs/fileResolver.h>

// engine includes
#include <wallpaperengine/irrlicht.h>

namespace wp
{
    namespace fs
    {
        fileResolver::fileResolver ()
        {
            this->m_environment.push_back (".");
        }

        fileResolver::fileResolver (std::vector<irr::io::path> environment)
        {
            this->m_environment.insert (this->m_environment.end (), environment.begin (), environment.end ());
        }

        void fileResolver::appendEnvironment (irr::io::path path)
        {
            this->m_environment.push_back (wp::irrlicht::device->getFileSystem ()->getAbsolutePath (path));
        }

        void fileResolver::removeEnvironment(irr::io::path path)
        {
            std::vector<irr::io::path>::const_iterator cur = this->m_environment.begin ();
            std::vector<irr::io::path>::const_iterator end = this->m_environment.end ();
            irr::io::path absolute = wp::irrlicht::device->getFileSystem ()->getAbsolutePath (path);

            for (; cur != end; cur ++)
            {
                if (*cur == path)
                {
                    this->m_environment.erase (cur);
                    return;
                }
            }
        }

        void fileResolver::prependEnvironment (irr::io::path path)
        {
            this->m_environment.insert (
                    this->m_environment.begin (),
                    wp::irrlicht::device->getFileSystem ()->getAbsolutePath (path)
            );
        }

        void fileResolver::changeWorkingDirectory (irr::io::path newpath)
        {
            this->m_environment.erase (this->m_environment.begin ());
            this->prependEnvironment (newpath);
        }

        irr::io::path fileResolver::getWorkingDirectory ()
        {
            return *this->m_environment.begin ();
        }

        fs::fileResolver fileResolver::clone ()
        {
            return fileResolver (this->m_environment);
        }

        irr::io::path fileResolver::resolve (irr::io::path name)
        {
            std::vector<irr::io::path>::const_iterator cur = this->m_environment.begin ();
            std::vector<irr::io::path>::const_iterator end = this->m_environment.end ();
            irr::io::path tmp = "";

            // try to resolve the path
            for (; cur != end; cur ++)
            {
                tmp = *cur;
                tmp += "/";
                tmp += name;

                if (wp::irrlicht::device->getFileSystem ()->existFile (tmp) == true)
                {
                    wp::irrlicht::device->getLogger ()->log ("Resolved file to", tmp.c_str ());

                    return tmp;
                }
            }

            wp::irrlicht::device->getLogger ()->log ("Failed resolving file ", name.c_str (), irr::ELL_ERROR);

            return "";
        }

        irr::io::path fileResolver::resolveOnWorkingDirectory (irr::io::path name)
        {
            irr::io::path file = *this->m_environment.begin () + "/" + name;

            if (wp::irrlicht::device->getFileSystem ()->existFile (file) == true)
            {
                wp::irrlicht::device->getLogger ()->log ("Resolved file in working directory to", file.c_str ());

                // perform stat over the file to check for directory
                return file;
            }

            wp::irrlicht::device->getLogger ()->log ("Failed resolving file on working directory for ", name.c_str (), irr::ELL_ERROR);

            return "";
        }

        irr::io::path fileResolver::resolve (json name)
        {
            std::string tmp = name;
            return this->resolve (tmp.c_str ());
        }

        irr::io::path fileResolver::resolveOnWorkingDirectory (json name)
        {
            std::string tmp = name;
            return this->resolveOnWorkingDirectory (tmp.c_str ());
        }

        irr::io::path fileResolver::resolve (const char* name)
        {
            return this->resolve (irr::io::path(name));
        }

        irr::io::path fileResolver::resolveOnWorkingDirectory (const char* name)
        {
            return this->resolveOnWorkingDirectory(irr::io::path (name));
        }

        fileResolver resolver;
    }
}