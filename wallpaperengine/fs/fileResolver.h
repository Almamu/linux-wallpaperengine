#ifndef WALLENGINE_RESOLVER_H
#define WALLENGINE_RESOLVER_H

#include <string>
#include <vector>
#include <irrlicht/path.h>

namespace wp
{
    namespace fs
    {
        class fileResolver
        {
        public:
            fileResolver ();
            fileResolver (std::vector<irr::io::path> environment);

            void appendEnvironment (irr::io::path path);
            void removeEnvironment (irr::io::path path);
            void changeWorkingDirectory (irr::io::path newpath);
            fileResolver clone ();
            irr::io::path resolve (irr::io::path name);

        protected:
            void prependEnvironment (irr::io::path path);

        private:
            std::vector<irr::io::path> m_environment;
        };

        static fileResolver resolver;
    }
}


#endif //WALLENGINE_RESOLVER_H
