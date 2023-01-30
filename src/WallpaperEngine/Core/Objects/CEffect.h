#pragma once

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Objects/Effects/CFBO.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"
#include "WallpaperEngine/Core/CObject.h"
#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"
#include "WallpaperEngine/Assets/CContainer.h"

namespace WallpaperEngine::Core
{
    class CObject;
};

namespace WallpaperEngine::Core::UserSettings
{
    class CUserSettingBoolean;
}

namespace WallpaperEngine::Core::Objects
{
    using json = nlohmann::json;
    using namespace WallpaperEngine::Assets;
    using namespace WallpaperEngine::Core::UserSettings;

    class CEffect
    {
    public:
        CEffect (
            std::string name,
            std::string description,
            std::string group,
            std::string preview,
            Core::CObject* object,
            CUserSettingBoolean* visible
        );

        static CEffect* fromJSON (json data, CUserSettingBoolean* visible, Core::CObject* object, const CContainer* container);

        const std::vector<std::string>& getDependencies () const;
        const std::vector<Images::CMaterial*>& getMaterials () const;
        const std::vector<Effects::CFBO*>& getFbos () const;
        bool isVisible () const;

        Effects::CFBO* findFBO (const std::string& name);
    protected:
        static void constantsFromJSON (json::const_iterator constants_it, Core::Objects::Images::Materials::CPass* pass);
        static void combosFromJSON (json::const_iterator combos_it, Core::Objects::Images::Materials::CPass* pass);
        static void fbosFromJSON (json::const_iterator fbos_it, CEffect* effect);
        static void dependencyFromJSON (json::const_iterator dependencies_it, CEffect* effect);
        static void materialsFromJSON (json::const_iterator passes_it, CEffect* effect, const CContainer* container);

        void insertDependency (const std::string& dep);
        void insertMaterial (Images::CMaterial* material);
        void insertFBO (Effects::CFBO* fbo);

    private:
        std::string m_name;
        std::string m_description;
        std::string m_group;
        std::string m_preview;
        Core::CObject* m_object;
        CUserSettingBoolean* m_visible;

        std::vector<std::string> m_dependencies;
        std::vector<Images::CMaterial*> m_materials;
        std::vector<Effects::CFBO*> m_fbos;
    };
}
