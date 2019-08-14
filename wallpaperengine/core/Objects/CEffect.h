#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

#include "Images/CMaterial.h"

namespace wp::core::Objects
{
    using json = nlohmann::json;

    class CEffect
    {
    public:
        CEffect (
            std::string name,
            std::string description,
            std::string group,
            std::string preview
        );

        static CEffect* fromJSON (json data);

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

        std::vector<std::string> m_dependencies;
        std::vector<Images::CMaterial*> m_materials;
    };
}
