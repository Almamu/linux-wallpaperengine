#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "../object.h"

namespace wp::core::objects
{
    using json = nlohmann::json;

    class sound : object
    {
    public:
        static object* fromJSON (
            json data,
            bool visible,
            irr::u32 id,
            std::string name,
            const irr::core::vector3df& origin,
            const irr::core::vector3df& scale,
            const irr::core::vector3df& angles
        );

        void insertSound (std::string filename);
        std::vector<std::string>* getSounds ();
    protected:

        sound (
            bool visible,
            irr::u32 id,
            std::string name,
            const irr::core::vector3df& origin,
            const irr::core::vector3df& scale,
            const irr::core::vector3df& angles
        );
    private:
        std::vector<std::string> m_sounds;
    };
}
