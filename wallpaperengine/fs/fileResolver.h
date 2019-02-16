/**
 * @author Alexis Maiquez Murcia <almamu@almamu.com>
 */
#ifndef WALLENGINE_RESOLVER_H
#define WALLENGINE_RESOLVER_H

#include <string>
#include <vector>
#include <irrlicht/path.h>
#include <nlohmann/json.hpp>

namespace wp
{
    using json = nlohmann::json;

    namespace fs
    {
        /**
         * Custom file resolver to limit our searches to specific folders
         */
        class fileResolver
        {
        public:
            /**
             * Basic constructor, uses current path as base folder
             */
            fileResolver ();
            /**
             * Creates a file resolver using a list as environment folders
             *
             * @param environment List of directories to use for finding files
             */
            fileResolver (std::vector<irr::io::path> environment);

            /**
             * Adds a new folder to the environment list
             *
             * @param path
             */
            void appendEnvironment (irr::io::path path);
            /**
             * Removes the given folder from the environment list
             *
             * @param path
             */
            void removeEnvironment (irr::io::path path);
            /**
             * Updates the current working directory
             *
             * @param newpath
             */
            void changeWorkingDirectory (irr::io::path newpath);
            /**
             * @return The current working directory
             */
            irr::io::path getWorkingDirectory ();
            /**
             * @return A clone of this fileResolver
             */
            fileResolver clone ();
            /**
             * Resolves the given filename or relative path
             * on the current working directory or any of the registered
             * environment folders
             *
             * @param name The filename/relative path to resolve
             *
             * @return The full path to the file
             */
            irr::io::path resolve (irr::io::path name);
            /**
             * Resolves the given filename or relative path
             * on the current working directory only
             *
             * @param name The filename/relative path to resolve
             *
             * @return  The full path to the file
             */
            irr::io::path resolveOnWorkingDirectory (irr::io::path name);
            irr::io::path resolve (json name);
            irr::io::path resolveOnWorkingDirectory (json name);
            irr::io::path resolve (const char* name);
            irr::io::path resolveOnWorkingDirectory (const char* name);

        protected:
            void prependEnvironment (irr::io::path path);

        private:
            /**
             * List of environment paths in which the resolver will look for
             * files in when trying to resolve them
             */
            std::vector<irr::io::path> m_environment;
        };

        /**
         * Static resolver used trough most of the application
         */
        extern fileResolver resolver;
    };
}


#endif //WALLENGINE_RESOLVER_H
