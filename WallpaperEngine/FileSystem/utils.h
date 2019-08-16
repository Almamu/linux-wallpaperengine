/**
 * @author Alexis Maiquez Murcia <almamu@almamu.com>
 */
#ifndef WALLENGINE_RESOLVER_H
#define WALLENGINE_RESOLVER_H

#include <string>
#include <vector>
#include <irrlicht/path.h>
#include <nlohmann/json.hpp>

namespace WallpaperEngine::FileSystem
{
    /**
     * Loads a full file into an std::string
     *
     * @param file
     * @return
     */
    std::string loadFullFile (irr::io::path file);
}


#endif //WALLENGINE_RESOLVER_H
