/**
 * @author Alexis Maiquez Murcia <almamu@almamu.com>
 */
#ifndef WALLENGINE_RESOLVER_H
#define WALLENGINE_RESOLVER_H

#include <string>
#include <vector>
#include <irrlicht/path.h>
#include <nlohmann/json.hpp>

namespace WallpaperEngine
{
    using json = nlohmann::json;

    namespace fs
    {
        /**
         * Custom file resolver to limit our searches to specific folders
         */
        class utils
        {
        public:
            /**
             * Loads a full file into an std::string
             *
             * @param file
             * @return
             */
            static std::string loadFullFile (irr::io::path file);
        };
    };
}


#endif //WALLENGINE_RESOLVER_H
