#pragma once

#include <iostream>
#include <map>
#include <vector>

#include "WallpaperEngine/Assets/CContainer.h"
#include "WallpaperEngine/Assets/ITexture.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

#include "CShaderUnit.h"
#include "CGLSLContext.h"

#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Render::Shaders {
using json = nlohmann::json;
using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Data::Model;

/**
 * A basic shader loader that adds basic function definitions to every loaded shader
 */
class CShader {
  public:
    struct ParameterSearchResult {
        Variables::CShaderVariable* vertex;
        Variables::CShaderVariable* fragment;
    };
    /**
     * Compiler constructor, loads the given shader file and prepares
     * the pre-processing and compilation of the shader, adding
     * required definitions if needed
     *
     * @param container The container to use for file lookup
     * @param filename The file to load
     * @param type The type of shader
     * @param combos Settings for the shader
     * @param foundCombos The list of currently defined combos
     * @param textures The list of available textures for the shader
     * @param constants Default values for shader variables
     * @param recursive Whether the compiler should add base definitions or not
     */
    CShader (
        const CContainer& container, std::string filename,
        const ComboMap& combos, const ComboMap& overrideCombos,
        const TextureMap& textures, const TextureMap& overrideTextures,
        const ShaderConstantMap& constants);
    /**
     * @return The vertex's shader coude for OpenGL to use
     */
    const std::string& vertex ();
    /**
     * @return The fragment's shader code for OpenGL to use
     */
    const std::string& fragment ();
    /**
     * @return The vertex shader unit
     */
    [[nodiscard]] const CShaderUnit& getVertex () const;
    /**
     * @return The fragment shader unit
     */
    [[nodiscard]] const CShaderUnit& getFragment () const;
    /**
     * @return The list of combos available for this shader after compilation
     */
    [[nodiscard]] const std::map<std::string, int>& getCombos () const;
    /**
     * Searches for the given parameter in the two shader units and return whatever was found
     *
     * @param name
     * @return
     */
    [[nodiscard]] ParameterSearchResult findParameter (const std::string& name);

  private:
    /**
     * The vertex shader unit used in this shader
     */
    CShaderUnit m_vertex;
    /**
     * The fragment shader unit used in this shader
     */
    CShaderUnit m_fragment;
    /**
     * The shader file this instance is loading
     */
    std::string m_file = "";
    /**
     * The parameters the shader needs
     */
    std::vector<Variables::CShaderVariable*> m_parameters = {};
    /**
     * The combos the shader should be generated with
     */
    const ComboMap& m_combos;
    /**
     * The overriden combos
     */
    const ComboMap& m_overrideCombos;
    /**
     * The list of textures the pass knows about
     */
    const TextureMap m_passTextures;
    /**
     * The list of the override textures
     */
    const TextureMap& m_overrideTextures;
};
} // namespace WallpaperEngine::Render::Shaders
