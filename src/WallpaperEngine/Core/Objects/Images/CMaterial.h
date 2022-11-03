#pragma once

#include "WallpaperEngine/Core/Objects/Images/Materials/CPass.h"
#include "WallpaperEngine/Core/Objects/Effects/CBind.h"

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Assets/CContainer.h"

namespace WallpaperEngine::Core::Objects::Images
{
    using json = nlohmann::json;
    using namespace WallpaperEngine::Assets;

    class CMaterial
    {
    public:
        static CMaterial* fromFile (const std::string& filename, const CContainer* container);
        static CMaterial* fromJSON (const std::string& name, json data);
        static CMaterial* fromFile (const std::string& filename, const std::string& target, const CContainer* container);
        static CMaterial* fromJSON (const std::string& name, json data, const std::string& target);

        void insertPass (Materials::CPass* mass);
        void insertTextureBind (Effects::CBind* bind);

        const std::vector <Materials::CPass*>& getPasses () const;
        const std::map <int, Effects::CBind*>& getTextureBinds () const;
        const std::string& getTarget () const;
        const bool hasTarget () const;
        const std::string& getName () const;
    protected:
        CMaterial (const std::string& name);

        void setTarget (const std::string& target);
    private:
        std::vector <Materials::CPass*> m_passes;
        std::map <int, Effects::CBind*> m_textureBindings;
        std::string m_target;
        std::string m_name;
    };
};
