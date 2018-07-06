#include <sys/stat.h>
#include <wallpaperengine/irrlicht.h>
#include "fileResolver.h"

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
            this->m_environment.push_back (".");
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

        fileResolver fileResolver::clone ()
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

                struct stat buffer;

                if (stat (tmp.c_str (), &buffer) == 0)
                {
                    wp::irrlicht::device->getLogger ()->log ("Resolved file to", tmp.c_str ());
                    return tmp;
                }
            }

            wp::irrlicht::device->getLogger ()->log ("Failed resolving file ", name.c_str ());

            return "";
        }
    }
}