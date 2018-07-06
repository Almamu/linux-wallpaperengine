#ifndef WALLENGINE_CONFIG_H
#define WALLENGINE_CONFIG_H

#include <string>
#include <irrlicht/path.h>

namespace wp
{
    namespace config
    {
        class path
        {
        public:
            static irr::io::path resources;
            static irr::io::path base;
            static irr::io::path shaders;
        };
    };
}


#endif //WALLENGINE_CONFIG_H
