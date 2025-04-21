#pragma once

#include "WallpaperEngine/Core/Objects/Effects/CBind.h"

#include "WallpaperEngine/Assets/CContainer.h"
#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"

namespace WallpaperEngine::Core::Objects::Images::Materials {
class CPass;
}

namespace WallpaperEngine::Core::Objects::Images {
using json = nlohmann::json;
using namespace WallpaperEngine::Assets;

/**
 * Represents a material in use in the background
 */
class CMaterial {
  public:
    struct OverrideInfo {
        std::map<std::string, int> combos;
        std::map<std::string, const Core::Objects::Effects::Constants::CShaderConstant*> constants;
        std::map<int, std::string> textures;
    };

    static const CMaterial* fromFile (
        const std::filesystem::path& filename, const Assets::CContainer* container, bool solidlayer = false,
        std::map<int, const Effects::CBind*> textureBindings = {}, const OverrideInfo* overrides = nullptr);
    static const CMaterial* fromFile (
        const std::filesystem::path& filename, const std::string& target, const Assets::CContainer* container,
        bool solidlayer = false, std::map<int, const Effects::CBind*> textureBindings = {},
        const OverrideInfo* overrides = nullptr);
    static const CMaterial* fromJSON (
        const std::string& name, const json& data, bool solidlayer = false,
        std::map<int, const Effects::CBind*> textureBindings = {}, const OverrideInfo* overrides = nullptr);
    static const CMaterial* fromJSON (
        const std::string& name, const json& data, const std::string& target, bool solidlayer = false,
        std::map<int, const Effects::CBind*> textureBindings = {}, const OverrideInfo* overrides = nullptr);
    static const CMaterial* fromCommand (const json& data);

    /**
     * @return All the rendering passes that happen for this material
     */
    [[nodiscard]] const std::vector<const Materials::CPass*>& getPasses () const;
    /**
     * @return The textures that have to be bound while rendering the material.
     * 		   These act as an override of the textures specified by the parent effect
     */
    [[nodiscard]] const std::map<int, const Effects::CBind*>& getTextureBinds () const;
    /**
     * @return The materials destination (fbo) if required
     */
    [[nodiscard]] const std::string& getTarget () const;
    /**
     * @return Indicates if this material has a specific destination (fbo) while rendering
     */
    [[nodiscard]] bool hasTarget () const;
    /**
     * @return The name of the material
     */
    [[nodiscard]] const std::string& getName () const;
    /**
     * @return If this material is a solidlayer or not
     */
    [[nodiscard]] bool isSolidLayer () const;

  protected:
    CMaterial (
        std::string name, bool solidlayer, std::map<int, const Effects::CBind*> textureBindings,
        std::vector<const Materials::CPass*> passes);
    CMaterial (
        std::string name, std::string target, bool solidlayer, std::map<int, const Effects::CBind*> textureBindings,
        std::vector<const Materials::CPass*> passes);

  private:
    /** All the shader passes required to render this material */
    const std::vector<const Materials::CPass*> m_passes;
    /** List of texture bind overrides to use for this material */
    const std::map<int, const Effects::CBind*> m_textureBindings;
    /** The FBO target to render to (if any) */
    const std::string m_target;
    /** The material's name */
    const std::string m_name;
    /** If this material is a solid layer or not */
    const bool m_solidlayer;
};
} // namespace WallpaperEngine::Core::Objects::Images
