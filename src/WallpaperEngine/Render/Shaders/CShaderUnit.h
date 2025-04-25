#pragma once

#include <map>
#include <memory>
#include <string>

#include "CGLSLContext.h"
#include "WallpaperEngine/Assets/CContainer.h"
#include "WallpaperEngine/Core/Core.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstant.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"
#include "nlohmann/json.hpp"

namespace WallpaperEngine::Render::Shaders {
using json = nlohmann::json;
using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Core::Objects::Effects::Constants;
/**
 * Represents a whole shader unit
 */
class CShaderUnit {
  public:
    CShaderUnit (
        CGLSLContext::UnitType type, std::string file, std::string content, std::shared_ptr<const CContainer> container,
        const std::map<std::string, const CShaderConstant*>& constants, const std::map<int, std::string>& passTextures,
        const std::map<std::string, int>& combos);
    ~CShaderUnit () = default;

    /**
     * Links this shader unit with another unit so they're treated as one
     *
     * @param unit
     */
    void linkToUnit (const CShaderUnit* unit);
    /**
     * @return The shader unit linked to this unit (if any)
     */
    [[nodiscard]] const CShaderUnit* getLinkedUnit () const;

    /**
     * @return The unit's source code already compiled and ready to be used by OpenGL
     */
    [[nodiscard]] const std::string& compile ();

    /**
     * @return The parameters the shader unit has as input
     */
    [[nodiscard]] const std::vector<Variables::CShaderVariable*>& getParameters () const;
    /**
     * @return The textures this shader unit requires
     */
    [[nodiscard]] const std::map<int, std::string>& getTextures () const;
    /**
     * @return The combos set for this shader unit by the configuration
     */
    [[nodiscard]] const std::map<std::string, int>& getCombos () const;
    /**
     * @return Other combos detected by this shader unit during the preprocess
     */
    [[nodiscard]] const std::map<std::string, int>& getDiscoveredCombos () const;

  protected:
    /**
     * Extracts any and all possible shader combo configurations
     * available in this shader unit, prepares includes
     * and lays the ground for the actual code to be ready
     */
    void preprocess ();

  private:
    /**
     * Parses the input shader looking for possible combo values that are required for it to properly work
     */
    void preprocessVariables ();
    /**
     * Parses the input shader looking for include directives to extract the full list of included files
     */
    void preprocessIncludes ();
    /**
     * Parses the input shader lookin for require directives to comment them out for now
     */
    void preprocessRequires ();

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
     * The type of shder unit we have
     */
    CGLSLContext::UnitType m_type;
    /**
     * The filename of this shader unit
     */
    std::string m_file;
    /**
     * Shader's original contents
     */
    std::string m_content;
    /**
     * Includes content to be added on compilation
     */
    std::string m_includes;
    /**
     * Shader's content after the preprocessing step
     */
    std::string m_preprocessed;
    /**
     * Shader's code after the compilation of glslang and spirv
     */
    std::string m_final;
    /**
     * The parameters the shader needs
     */
    std::vector<Variables::CShaderVariable*> m_parameters;
    /**
     * Pre-defined values for the combos
     */
    const std::map<std::string, int>& m_combos;
    /**
     * The combos discovered in the pre-processing step that were not in the combos list
     */
    std::map<std::string, int> m_discoveredCombos;
    /**
     * The combos used by this unit that should be added
     */
    std::map<std::string, bool> m_usedCombos;
    /**
     * The constants defined for this unit
     */
    const std::map<std::string, const CShaderConstant*>& m_constants;
    /** The textures that are already applied to this shader */
    const std::map<int, std::string> m_passTextures;
    /** The default textures to use when a texture is not applied in a given slot */
    std::map<int, std::string> m_defaultTextures;
    /**
     * The shader unit this unit is linked to
     */
    const CShaderUnit* m_link;
    /**
     * The container to source files from
     */
    const std::shared_ptr<const CContainer> m_container;
};
}
