#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

#include "WallpaperEngine/Core/CObject.h"
#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"

namespace WallpaperEngine::Core
{
    class CObject;
};

namespace WallpaperEngine::Core::Objects
{
    using json = nlohmann::json;

    class CEffect
    {
    public:
        CEffect (
            std::string name,
            std::string description,
            std::string group,
            std::string preview,
            Core::CObject* object
        );

        static CEffect* fromJSON (json data, Core::CObject* object);

        std::vector<std::string>* getDependencies ();
        std::vector<Images::CMaterial*>* getMaterials ();
    protected:
        void insertDependency (const std::string& dep);
        void insertMaterial (Images::CMaterial* material);
    private:
        std::string m_name;
        std::string m_description;
        std::string m_group;
        std::string m_preview;
        Core::CObject* m_object;

        std::vector<std::string> m_dependencies;
        std::vector<Images::CMaterial*> m_materials;
    };
}
