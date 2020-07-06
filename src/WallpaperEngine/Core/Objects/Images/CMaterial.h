#pragma once

#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/Objects/Images/Materials/CPass.h"
#include "WallpaperEngine/Core/Objects/Effects/CBind.h"

#include "WallpaperEngine/Core/Core.h"

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

        void insertPass (Materials::CPass* mass);
        void insertTextureBind (Effects::CBind* bind);

        const std::vector <Materials::CPass*>& getPasses () const;
        const std::vector <Effects::CBind*>& getTextureBinds () const;
        const std::string& getTarget () const;
        const bool hasTarget () const;
    protected:
        CMaterial ();

        void setTarget (const std::string& target);
    private:
        std::vector <Materials::CPass*> m_passes;
        std::vector <Effects::CBind*> m_textureBindings;
        std::string m_target;
    };
};
