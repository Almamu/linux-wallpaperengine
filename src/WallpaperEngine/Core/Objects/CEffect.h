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
}

namespace WallpaperEngine::Core::UserSettings
{
    class CUserSettingBoolean;
}

namespace WallpaperEngine::Core::Objects
{
    using json = nlohmann::json;
    using namespace WallpaperEngine::Assets;
    using namespace WallpaperEngine::Core::UserSettings;

    /**
     * Represents an effect applied to background objects
     */
    class CEffect
    {
    public:
        CEffect (
            std::string name,
            std::string description,
            std::string group,
            std::string preview,
            CObject* object,
            CUserSettingBoolean* visible
        );

        static CEffect* fromJSON (json data, UserSettings::CUserSettingBoolean* visible, CObject* object, CContainer* container);

        /**
         * @return List of dependencies for the effect to work
         */
        [[nodiscard]] const std::vector<std::string>& getDependencies () const;
        /**
         * @return List of materials the effect applies
         */
        [[nodiscard]] const std::vector<Images::CMaterial*>& getMaterials () const;
        /**
         * @return The list of FBOs to be used for this effect
         */
        [[nodiscard]] const std::vector<Effects::CFBO*>& getFbos () const;
        /**
         * @return If the effect is visible or not
         */
        [[nodiscard]] bool isVisible () const;
        /**
         * Searches the FBOs list for the given FBO
         *
         * @param name The FBO to search for
         *
         * @return
         */
        Effects::CFBO* findFBO (const std::string& name);
    protected:
        static void constantsFromJSON (json::const_iterator constants_it, Core::Objects::Images::Materials::CPass* pass);
        static void combosFromJSON (json::const_iterator combos_it, Core::Objects::Images::Materials::CPass* pass);
        static void fbosFromJSON (json::const_iterator fbos_it, CEffect* effect);
        static void dependencyFromJSON (json::const_iterator dependencies_it, CEffect* effect);
        static void materialsFromJSON (json::const_iterator passes_it, CEffect* effect, CContainer* container);

        void insertDependency (const std::string& dep);
        void insertMaterial (Images::CMaterial* material);
        void insertFBO (Effects::CFBO* fbo);

    private:
        /** Effect's name */
        std::string m_name;
        /** Effect's description used in the UI */
        std::string m_description;
        /** Effect's group used in the UI */
        std::string m_group;
        /** A project that previews the given effect, used in the UI */
        std::string m_preview;
        /** The object the effect applies to */
        CObject* m_object;
        /** If the effect is visible or not */
        UserSettings::CUserSettingBoolean* m_visible;

        /** List of dependencies for the effect */
        std::vector<std::string> m_dependencies;
        /** List of materials the effect applies */
        std::vector<Images::CMaterial*> m_materials;
        /** List of FBOs required for this effect */
        std::vector<Effects::CFBO*> m_fbos;
    };
}
