#pragma once

#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Objects/Images/CMaterial.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"

namespace WallpaperEngine::Core::Objects {
class CEffect;
}

namespace WallpaperEngine::Core::Objects::Images {
class CMaterial;
}

namespace WallpaperEngine::Core::Objects::Images::Materials {
using json = nlohmann::json;

/**
 * Represents a shader pass of an object
 */
class CPass {
    friend class WallpaperEngine::Core::Objects::CEffect;

  public:
    static const CPass* fromJSON (const json& data, const CMaterial::OverrideInfo* overrides);

    /**
     * @return The list of textures to bind while rendering
     */
    [[nodiscard]] const std::map<int, std::string>& getTextures () const;
    /**
     * @return Shader constants that alter how the shader should behave
     */
    [[nodiscard]] const std::map<std::string, const Effects::Constants::CShaderConstant*>& getConstants () const;
    /**
     * @return Shader combos that alter how the shader should behave
     */
    [[nodiscard]] const std::map<std::string, int>& getCombos () const;
    /**
     * @return Shader to be used while rendering the pass
     */
    [[nodiscard]] const std::string& getShader () const;
    /**
     * @return The blending mode to use while rendering
     */
    [[nodiscard]] const std::string& getBlendingMode () const;
    /**
     * @return The culling mode to use while rendering
     */
    [[nodiscard]] const std::string& getCullingMode () const;
    /**
     * @return If depth testing has to happen while rendering
     */
    [[nodiscard]] const std::string& getDepthTest () const;
    /**
     * @return If depth write has to happen while rendering
     */
    [[nodiscard]] const std::string& getDepthWrite () const;

  protected:
    CPass (std::string blending, std::string cullmode, std::string depthtest, std::string depthwrite,
           std::string shader, std::map<int, std::string> textures, std::map<std::string, int> combos,
           std::map<std::string, const Core::Objects::Effects::Constants::CShaderConstant*> constants);

  private:
    // TODO: CREATE ENUMERATIONS FOR THESE INSTEAD OF USING STRING VALUES!
    /** The blending mode to use */
    const std::string m_blending;
    /** The culling mode to use */
    const std::string m_cullmode;
    /** If depthtesting has to happen while drawing */
    const std::string m_depthtest;
    /** If depthwrite has to happen while drawing */
    const std::string m_depthwrite;
    /** The shader to use */
    const std::string m_shader;
    /** The list of textures to use */
    const std::map<int, std::string> m_textures;
    /** Different combo settings for shader input */
    const std::map<std::string, int> m_combos;
    /** Shader constant values to use for  the shaders */
    const std::map<std::string, const Core::Objects::Effects::Constants::CShaderConstant*> m_constants;
};
} // namespace WallpaperEngine::Core::Objects::Images::Materials
