#pragma once

#include <iostream>
#include <map>
#include <vector>

#include "WallpaperEngine/Assets/CContainer.h"
#include "WallpaperEngine/Assets/ITexture.h"
#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

#include "CGLSLContext.h"

namespace WallpaperEngine::Render::Shaders {
using json = nlohmann::json;
using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Core::Objects::Effects::Constants;

/**
 * A basic shader loader that adds basic function definitions to every loaded shader
 */
class CCompiler {
  public:
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
    CCompiler (CContainer* container, std::string filename, CGLSLContext::ShaderType type, std::map<std::string, int>* combos,
              std::map<std::string, bool>* foundCombos, const std::vector<std::string>& textures,
              const std::map<std::string, CShaderConstant*>& constants);
    /**
     * Pre-processes the shader to detect variables, process includes and other small things WallpaperEngine
     * does to shaders before actually using them
     */
    void precompile ();
    /**
     * Performs the final processing of the shader doing a few last transformations through glslang
     */
    std::string compile ();
    /**
     * Searches the list of parameters available for the parameter with the given value
     *
     * @param identifier The identifier to search for
     * @return The shader information
     */
    Variables::CShaderVariable* findParameter (const std::string& identifier);
    /**
     * @return The list of parameters available for this shader with their default values
     */
    [[nodiscard]] const std::vector<Variables::CShaderVariable*>& getParameters () const;
    /**
     * @return The list of combos available for this shader after compilation
     */
    [[nodiscard]] std::map<std::string, int>* getCombos () const;
    /**
     * @return The list of textures inferred from the shader's code
     */
    [[nodiscard]] const std::map<int, std::string>& getTextures () const;

  private:
    /**
     * Parses a COMBO value to add the proper define to the code
     *
     * @param content The parameter configuration
     * @param defaultValue
     */
    void parseComboConfiguration (const std::string& content, int defaultValue = 0);
    /**
     * Parses a parameter extra metadata created by wallpaper engine
     *
     * @param type The type of variable to parse
     * @param name The name of the variable in the shader (for actual variable declaration)
     * @param content The parameter configuration
     */
    void parseParameterConfiguration (const std::string& type, const std::string& name, const std::string& content);
    /**
     * Tries to find the given shader file and compile it
     *
     * @param filename The shader's filename
     *
     * @return The compiled contents
     */
    std::string lookupShaderFile (const std::string& filename);

    /**
     * The shader file this instance is loading
     */
    std::string m_file;
    /**
     * The original file content
     */
    std::string m_content;
    /**
     * The final, compiled content ready to be used by OpenGL
     */
    std::string m_processedContent;
    /**
     * The contents of all the included files
     */
    std::string m_includeContent;
    /**
     * The type of shader
     */
    CGLSLContext::ShaderType m_type;
    /**
     * The parameters the shader needs
     */
    std::vector<Variables::CShaderVariable*> m_parameters;
    /**
     * The combos the shader should be generated with
     */
    std::map<std::string, int>* m_combos;

    /**
     * Combos that come from the pass' chain that should be added
     */
    std::map<std::string, int> m_baseCombos;

    /**
     * The combos the shader code has defined (shared between fragment and vertex)
     */
    std::map<std::string, bool>* m_foundCombos;

    /**
     * The list of textures the pass knows about
     */
    const std::vector<std::string> m_passTextures;
    /**
     * The shader constants with values for variables inside the shader
     */
    const std::map<std::string, CShaderConstant*>& m_constants;
    /**
     * The container to load files from
     */
    CContainer* m_container;
    /**
     * List of textures that the shader expects (inferred from sampler2D and it's JSON data)
     */
    std::map<int, std::string> m_textures;
};
} // namespace WallpaperEngine::Render::Shaders
