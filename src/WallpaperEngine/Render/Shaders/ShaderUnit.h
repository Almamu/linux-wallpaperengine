#pragma once

#include <map>
#include <memory>
#include <string>

#include "GLSLContext.h"
#include "WallpaperEngine/Data/JSON.h"
#include "WallpaperEngine/Render/Shaders/Variables/ShaderVariable.h"
#include "nlohmann/json.hpp"

#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Render::Shaders {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::FileSystem;
using namespace WallpaperEngine::Data::Model;

/**
 * Represents a whole shader unit
 */
class ShaderUnit {
  public:
    ShaderUnit (
        GLSLContext::UnitType type, std::string file, std::string content, const Container& container,
        const ShaderConstantMap& constants, const TextureMap& passTextures,
        const TextureMap& overrideTextures, const ComboMap& combos, const ComboMap& overrideCombos);
    ~ShaderUnit () = default;

    /**
     * Links this shader unit with another unit so they're treated as one
     *
     * @param unit
     */
    void linkToUnit (const ShaderUnit* unit);
    /**
     * @return The shader unit linked to this unit (if any)
     */
    [[nodiscard]] const ShaderUnit* getLinkedUnit () const;

    /**
     * @return The unit's source code already compiled and ready to be used by OpenGL
     */
    [[nodiscard]] const std::string& compile ();

    /**
     * @return The parameters the shader unit has as input
     */
    [[nodiscard]] const std::vector<Variables::ShaderVariable*>& getParameters () const;
    /**
     * @return The textures this shader unit requires
     */
    [[nodiscard]] const TextureMap& getTextures () const;
    /**
     * @return The combos set for this shader unit by the configuration
     */
    [[nodiscard]] const ComboMap& getCombos () const;
    /**
     * @return Other combos detected by this shader unit during the preprocess
     */
    [[nodiscard]] const ComboMap& getDiscoveredCombos () const;

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
    GLSLContext::UnitType m_type;
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
    std::vector<Variables::ShaderVariable*> m_parameters = {};
    /**
     * Pre-defined values for the combos
     */
    const ComboMap& m_combos;
    /**
     * Pre-defined overriden values for the combos
     */
    const ComboMap& m_overrideCombos;
    /**
     * The combos discovered in the pre-processing step that were not in the combos list
     */
    ComboMap m_discoveredCombos = {};
    /**
     * The combos used by this unit that should be added
     */
    std::map<std::string, bool> m_usedCombos = {};
    /**
     * The constants defined for this unit
     */
    const ShaderConstantMap& m_constants;
    /** The textures that are already applied to this shader */
    const TextureMap& m_passTextures;
    /** The textures that are being overridden */
    const TextureMap& m_overrideTextures;
    /** The default textures to use when a texture is not applied in a given slot */
    TextureMap m_defaultTextures = {};
    /**
     * The shader unit this unit is linked to
     */
    const ShaderUnit* m_link;
    /**
     * The container to source files from
     */
    const Container& m_container;
};
}
