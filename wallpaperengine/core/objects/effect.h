#pragma once

#include <nlohmann/json.hpp>
#include <irrlicht/irrlicht.h>

#include "images/material.h"

namespace wp::core::objects
{
    using json = nlohmann::json;

    class effect
    {
    public:
        effect (
            std::string name,
            std::string description,
            std::string group,
            std::string preview
        );

        static effect* fromJSON (json data);

        std::vector<std::string>* getDependencies ();
        std::vector<images::material*>* getMaterials ();
    protected:
        void insertDependency (const std::string& dep);
        void insertMaterial (images::material* material);
    private:
        std::string m_name;
        std::string m_description;
        std::string m_group;
        std::string m_preview;

        std::vector<std::string> m_dependencies;
        std::vector<images::material*> m_materials;
    };
}
