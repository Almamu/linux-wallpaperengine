#ifndef WALLENGINE_RESOLVER_H
#define WALLENGINE_RESOLVER_H

#include <string>
#include <vector>
#include <irrlicht/path.h>
#include <nlohmann/json.hpp>

namespace wp
{
    using json = nlohmann::json;
    class fs
    {
    public:
        class fileResolver
        {
        public:
            fileResolver ();
            fileResolver (std::vector<irr::io::path> environment);

            void appendEnvironment (irr::io::path path);
            void removeEnvironment (irr::io::path path);
            void changeWorkingDirectory (irr::io::path newpath);
            irr::io::path getWorkingDirectory ();
            fileResolver clone ();
            irr::io::path resolve (irr::io::path name);
            irr::io::path resolveOnWorkingDirectory (irr::io::path name);
            irr::io::path resolve (json name);
            irr::io::path resolveOnWorkingDirectory (json name);
            irr::io::path resolve (const char* name);
            irr::io::path resolveOnWorkingDirectory (const char* name);

        protected:
            void prependEnvironment (irr::io::path path);

        private:
            std::vector<irr::io::path> m_environment;
        };

        static fileResolver resolver;
    };
}


#endif //WALLENGINE_RESOLVER_H
