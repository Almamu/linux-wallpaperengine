#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "GLSLContext.h"
#include "WallpaperEngine/Assets/AssetLocator.h"
#include "WallpaperEngine/Data/JSON.h"

#include "WallpaperEngine/Data/Model/Types.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class ShaderVariable;
}

namespace WallpaperEngine::Render::Shaders {
using JSON = WallpaperEngine::Data::JSON::JSON;
using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Data::Model;

/**
 * Represents a whole shader unit
 */
class ShaderUnit {
public:
    ShaderUnit (
	const GLSLContext::UnitType type, std::string file, std::string content, const AssetLocator& assetLocator,
	const ShaderConstantMap& constants, const TextureMap& passTextures, const TextureMap& overrideTextures,
	const ComboMap& combos, const ComboMap& overrideCombos
    );
    ~ShaderUnit ();

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
    [[nodiscard]] bool requestsTextureFormat (size_t index) const;
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
     * Parses the input shader looking for require directives and resolves them into generated code
     */
    void preprocessRequires ();
    /** Native varying declarations ignore a trailing component selection on their name. */
    void preprocessVaryingDeclarations ();
    /** Rename authored identifiers reserved by GLSL before recording uniform names. */
    void preprocessReservedIdentifiers ();
    /**
     * Resolves a #require module name to generated GLSL code
     *
     * @param moduleName The module to resolve (e.g. "LightingV1")
     * @return Generated GLSL code for the module, or empty string if unknown
     */
    [[nodiscard]] std::string resolveRequireModule (const std::string& moduleName) const;
    /**
     * Generates the LightingV1 module stub (PerformLighting_V1 function)
     *
     * @return GLSL code defining PerformLighting_V1
     */
    [[nodiscard]] std::string generateLightingV1 () const;
    /**
     * Adjusts vertex varyings when a workshop shader declares a narrower vertex type than its fragment peer.
     */
    [[nodiscard]] std::string applyLinkedVaryingCompatibility (std::string source) const;
    /**
     * Declares fragment inputs used by the fragment unit and exported by the linked vertex unit,
     * but omitted from the authored fragment declarations.
     */
    [[nodiscard]] std::string applyMissingFragmentVaryingCompatibility (std::string source) const;
    /**
     * Adjusts fragment shaders that use wide texture coordinates as vec2 values in Wallpaper Engine effects.
     */
    [[nodiscard]] std::string applyFragmentTexCoordCompatibility (std::string source) const;
    /**
     * Removes const from initializers that depend on uniforms. Wallpaper Engine's shader
     * compiler accepts these while desktop GLSL requires a compile-time constant.
     */
    [[nodiscard]] std::string applyNonConstantInitializerCompatibility (std::string source) const;
    /**
     * Undefines a macro before an authored redefinition accepted by Wallpaper Engine's preprocessor.
     */
    [[nodiscard]] std::string applyDuplicateMacroCompatibility (std::string source) const;
    /**
     * Makes authored function names safe when they collide with generated header macros.
     */
    [[nodiscard]] std::string applyHeaderMacroCompatibility (std::string source) const;
    /**
     * Makes assigned shader inputs writable with local copies at the start of main(),
     * preserving the preprocessor conditions of their declarations.
     */
    [[nodiscard]] std::string applyWritableInputs (std::string source) const;
    /**
     * HLSL allows float/int values as ternary conditions; GLSL 330 requires bool.
     * Wraps bare-identifier ternary conditions in bool() in assignment and argument contexts.
     */
    [[nodiscard]] std::string applyFloatTernaryCompatibility (std::string source) const;
    /**
     * Makes Wallpaper Engine's implicit numeric conversions explicit at user-function calls.
     */
    [[nodiscard]] std::string applyNumericParameterCallCompatibility (std::string source) const;
    /**
     * Makes scalar splats, vector truncation, integer modulo, and numeric loop bounds explicit.
     */
    [[nodiscard]] std::string applyNumericInitializerCompatibility (std::string source) const;
    /**
     * Aligns vector widths passed to component-wise built-ins with strict GLSL overloads.
     */
    [[nodiscard]] std::string applyVectorBuiltinCompatibility (std::string source) const;
    /**
     * Converts multiplication of comparison results into an explicit numeric logical AND.
     */
    [[nodiscard]] std::string applyBooleanArithmeticCompatibility (std::string source) const;
    /**
     * Removes unmatched #endif directives (workshop shaders sometimes emit one extra #endif
     * that HLSL's preprocessor silently ignores but GLSL's does not).
     */
    void stripOrphanedEndifs ();
    /**
     * Debug aid: writes the fully composed unit source to $WPE_DUMP_SHADERS (if set),
     * including cache-hit variants, so every emitted unit can be inspected offline.
     */
    void dumpFinalSource () const;

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
    std::set<size_t> m_textureFormatRequests;
    /**
     * The shader unit this unit is linked to
     */
    const ShaderUnit* m_link;
    /**
     * The container to source files from
     */
    const AssetLocator& m_assetLocator;
};
}
