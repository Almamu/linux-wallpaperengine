#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "materials/passes.h"

namespace wp::core::objects::images
{
    using json = nlohmann::json;

    class material
    {
    public:
        static material* fromFile (irr::io::path filename);

        void insertPass (materials::passes* mass);

        std::vector <materials::passes*>* getPasses ();
    protected:
        material ();
    private:
        std::vector <materials::passes*> m_passes;
    };
};
