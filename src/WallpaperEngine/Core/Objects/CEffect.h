#pragma once

#include "WallpaperEngine/Assets/CContainer.h"
#include "WallpaperEngine/Core/CObject.h"
#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Objects/Effects/CFBO.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"
#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"

namespace WallpaperEngine::Core {
class CObject;
class CProject;
}

namespace WallpaperEngine::Core::UserSettings {
class CUserSettingBoolean;
}

namespace WallpaperEngine::Core::Objects {
using json = nlohmann::json;
using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Core::UserSettings;

/**
 * Represents an effect applied to background objects
 */
class CEffect {
  public:
    CEffect (
        std::string name, std::string description, std::string group, std::string preview, const CProject& project,
        const CUserSettingBoolean* visible, std::vector<std::string> dependencies,
        std::vector<const Effects::CFBO*> fbos, std::vector<const Images::CMaterial*> materials);

    static const CEffect* fromJSON (
        const json& data, const CUserSettingBoolean* visible, const CProject& object, const Images::CMaterial* material,
        const CContainer* container);

    /**
     * @return List of dependencies for the effect to work
     */
    [[nodiscard]] const std::vector<std::string>& getDependencies () const;
    /**
     * @return List of materials the effect applies
     */
    [[nodiscard]] const std::vector<const Images::CMaterial*>& getMaterials () const;
    /**
     * @return The list of FBOs to be used for this effect
     */
    [[nodiscard]] const std::vector<const Effects::CFBO*>& getFbos () const;
    /**
     * @return If the effect is visible or not
     */
    [[nodiscard]] bool isVisible () const;
    /**
     * @return The project this effect is part of
     */
    [[nodiscard]] const CProject& getProject () const;
    /**
     * Searches the FBOs list for the given FBO
     *
     * @param name The FBO to search for
     *
     * @return
     */
    const Effects::CFBO* findFBO (const std::string& name);

  protected:
    static std::map<std::string, const Core::Objects::Effects::Constants::CShaderConstant*> constantsFromJSON (
        const json::const_iterator& constants_it);
    static std::map<std::string, int> combosFromJSON (const json::const_iterator& combos_it);
    static std::vector<const Effects::CFBO*> fbosFromJSON (const json::const_iterator& fbos_it);
    static std::vector<std::string> dependenciesFromJSON (const json::const_iterator& dependencies_it);
    static std::vector<const Images::CMaterial*> materialsFromJSON (
        const json::const_iterator& passes_it, std::string name, const CContainer* container,
        std::map<int, Images::CMaterial::OverrideInfo>);
    static std::map<int, Images::CMaterial::OverrideInfo> overridesFromJSON (
        const json::const_iterator& passes_it, const Images::CMaterial* material);

  private:
    /** Effect's name */
    const std::string m_name;
    /** Effect's description used in the UI */
    const std::string m_description;
    /** Effect's group used in the UI */
    const std::string m_group;
    /** A project that previews the given effect, used in the UI */
    const std::string m_preview;
    /** If the effect is visible or not */
    const UserSettings::CUserSettingBoolean* m_visible;
    /** Project this effect is part of */
    const CProject& m_project;

    /** List of dependencies for the effect */
    const std::vector<std::string> m_dependencies;
    /** List of materials the effect applies */
    const std::vector<const Images::CMaterial*> m_materials;
    /** List of FBOs required for this effect */
    const std::vector<const Effects::CFBO*> m_fbos;
};
} // namespace WallpaperEngine::Core::Objects
