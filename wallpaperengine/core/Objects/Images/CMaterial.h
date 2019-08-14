#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "Materials/CPassess.h"

namespace wp::core::Objects::Images
{
    using json = nlohmann::json;

    class CMaterial
    {
    public:
        static CMaterial* fromFile (irr::io::path filename);
        static CMaterial* fromJSON (json data);

        void insertPass (Materials::CPassess* mass);

        std::vector <Materials::CPassess*>* getPasses ();
    protected:
        CMaterial ();
    private:
        std::vector <Materials::CPassess*> m_passes;
    };
};
