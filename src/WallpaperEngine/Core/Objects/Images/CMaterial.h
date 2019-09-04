#pragma once

#include <irrlicht/irrlicht.h>
#include <nlohmann/json.hpp>

#include "WallpaperEngine/Core/Objects/Images/Materials/CPassess.h"

namespace WallpaperEngine::Core::Objects::Images
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
