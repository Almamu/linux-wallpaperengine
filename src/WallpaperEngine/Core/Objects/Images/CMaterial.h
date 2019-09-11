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
        static CMaterial* fromFile (const irr::io::path& filename);
        static CMaterial* fromJSON (json data);
        static CMaterial* fromFile (const irr::io::path& filename, const std::string& target);
        static CMaterial* fromJSON (json data, const std::string& target);

        void insertPass (Materials::CPassess* mass);

        const std::vector <Materials::CPassess*>& getPasses () const;
        const std::string& getTarget () const;
        const bool hasTarget () const;
    protected:
        CMaterial ();

        void setTarget (const std::string& target);
    private:
        std::vector <Materials::CPassess*> m_passes;
        std::string m_target;
    };
};
