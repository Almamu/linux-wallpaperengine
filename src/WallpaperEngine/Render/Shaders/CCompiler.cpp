#include "common.h"
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

// shader compiler
#include <WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h>
#include <WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h>
#include <WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.h>
#include <WallpaperEngine/Render/Shaders/CCompiler.h>
#include <regex>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableFloat.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableInteger.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector2.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector3.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector4.h"

#include "CGLSLContext.h"
#include "WallpaperEngine/Assets/CAssetLoadException.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Assets;

namespace WallpaperEngine::Render::Shaders {
CCompiler::CCompiler (CContainer* container, std::string filename, CGLSLContext::ShaderType type, std::map<std::string, int>* combos,
                    std::map<std::string, bool>* foundCombos, const std::vector<std::string>& textures,
                    const std::map<std::string, CShaderConstant*>& constants) :
    m_combos (combos),
    m_foundCombos (foundCombos),
    m_passTextures (textures),
    m_type (type),
    m_file (std::move (filename)),
    m_constants (constants),
    m_container (container) {
    if (type == CGLSLContext::ShaderType_Vertex)
        this->m_content = this->m_container->readVertexShader (this->m_file);
    else if (type == CGLSLContext::ShaderType_Pixel)
        this->m_content = this->m_container->readFragmentShader (this->m_file);
    else if (type == CGLSLContext::ShaderType_Include)
        sLog.exception ("Include shaders should never be compiled, they're part of a bigger shader: ", this->m_file);

    // clone the combos into the baseCombos to keep track of values that must be embedded no matter what
    for (const auto& [name, value] : *this->m_combos)
        this->m_baseCombos.insert (std::make_pair (name, value));
}

std::string CCompiler::lookupShaderFile (const std::string& filename) {
    // TODO:
    // includes are not compiled, lookup the file and return the contents
    // there might be situations where an #include is part of a comment
    // instead of trying to figure out if an #include is part of that
    // just try to load it
    // if nothing is found, nothing is really lost
    // but if something is found, depending on the type of comment and the contents
    // the included file could break the shader, we'll need to perform some more
    // checks here at some point, but for now should be enough
    try {
        return this->m_container->readIncludeShader (filename);
    } catch (CAssetLoadException&) {
        sLog.error ("Cannot find include file ", filename, " in shader ", this->m_file, ". Using empty content.");
        return "";
    }
}

std::string& CCompiler::getCompiled () {
    return this->m_compiledContent;
}

void CCompiler::compile () {
    // reset include contents as the compilation requires this to be re-processed
    this->m_includeContent = "";
    std::string precompile = "#version 330\n"
                             "// ======================================================\n"
                             "// Processed shader " +
                             this->m_file +
                             "\n"
                             "// ======================================================\n"
                             "precision highp float;\n"
                             "#define mul(x, y) ((y) * (x))\n"
                             "#define max(x, y) max (y, x)\n"
                             "#define lerp mix\n"
                             "#define frac fract\n"
                             "#define CAST2(x) (vec2(x))\n"
                             "#define CAST3(x) (vec3(x))\n"
                             "#define CAST4(x) (vec4(x))\n"
                             "#define CAST3X3(x) (mat3(x))\n"
                             "#define saturate(x) (clamp(x, 0.0, 1.0))\n"
                             "#define texSample2D texture\n"
                             "#define texSample2DLod textureLod\n"
                             "#define atan2 atan\n"
                             "#define fmod(x, y) ((x)-(y)*trunc((x)/(y)))\n"
                             "#define ddx dFdx\n"
                             "#define ddy(x) dFdy(-(x))\n"
                             "#define GLSL 1\n\n";

    if (this->m_type == CGLSLContext::ShaderType_Vertex) {
        precompile += "#define attribute in\n"
                     "#define varying out\n";
    } else {
        precompile += "out vec4 out_FragColor;\n"
                     "#define varying in\n";
    }
    // searches for the combos available and adds the defines required

    // go line by line in the shader content
    size_t start = 0, end = 0;
    while ((end = this->m_content.find ('\n', start)) != std::string::npos) {
        // Extract a line from the string
        std::string line = this->m_content.substr (start, end - start);
        size_t combo = line.find("// [COMBO] ");
        size_t uniform = line.find("uniform ");
        size_t comment = line.find("// ");
        size_t semicolon = line.find(';');

        if (combo != std::string::npos) {
            this->parseComboConfiguration (line.substr(combo + strlen("// [COMBO] ")), 0);
        } else if (uniform != std::string::npos && comment != std::string::npos && semicolon != std::string::npos) {
            // uniforms with comments should never have a value assigned, use this fact to detect the required parts
            size_t last_space = line.find_last_of (' ', semicolon);

            if (last_space != std::string::npos) {
                size_t previous_space = line.find_last_of (' ', last_space - 1);

                if (previous_space != std::string::npos) {
                    // extract type and name
                    std::string type = line.substr (previous_space + 1, last_space - previous_space - 1);
                    std::string name = line.substr (last_space + 1, semicolon - last_space - 1);
                    std::string json = line.substr (comment + 2);

                    this->parseParameterConfiguration (type, name, json);
                }
            }
        }

        // Move to the next line
        start = end + 1;
    }

    // add all the defines we have for now
    for (const auto& [name, value] : *this->m_foundCombos) {
        // find the right value for the combo in the combos map
        auto combo = this->m_combos->find (name);

        if (combo == this->m_combos->end ())
            continue;

        precompile += "#define " + name + " " + std::to_string (combo->second) + "\n";
    }

    // add base combos that come from the pass change that MUST be added
    for (const auto& [name, value] : this->m_baseCombos) {
        auto alreadyFound = this->m_foundCombos->find (name);

        if (alreadyFound != this->m_foundCombos->end ())
            continue;

        precompile += "#define " + name + " " + std::to_string (value) + "\n";
    }

    precompile += this->m_content;

    // reset end so we start from the beginning
    end = 0;

    // then apply includes in-place
    while((start = precompile.find("#include", end)) != std::string::npos) {
        // TODO: CHECK FOR ERRORS HERE, MALFORMED INCLUDES WILL NOT BE PROPERLY HANDLED
        size_t quoteStart = precompile.find_first_of ('"', start) + 1;
        size_t quoteEnd = precompile.find_first_of('"', quoteStart);
        std::string filename = precompile.substr(quoteStart, quoteEnd - quoteStart);
        std::string content = this->lookupShaderFile (filename);

        this->m_includeContent += "// begin of include from file " + filename + "\n" +
            content +
            "\n// end of included from file " + filename + "\n";

        // replace the first two letters with a comment so the filelength doesn't change
        precompile = precompile.replace (start, 2, "//");

        // go to the end of the line
        end = quoteEnd + 1;
    }

    // include content might have more includes, so also handle those
    end = 0;

    // then apply includes in-place
    while((start = this->m_includeContent.find("#include", end)) != std::string::npos) {
        size_t lineEnd = this->m_includeContent.find_first_of ('\n', start);
        // TODO: CHECK FOR ERRORS HERE, MALFORMED INCLUDES WILL NOT BE PROPERLY HANDLED
        size_t quoteStart = this->m_includeContent.find_first_of ('"', start) + 1;
        size_t quoteEnd = this->m_includeContent.find_first_of('"', quoteStart);
        std::string filename = this->m_includeContent.substr(quoteStart, quoteEnd - quoteStart);
        std::string content = this->lookupShaderFile (filename);

        // file contents ready, replace things
        this->m_includeContent = this->m_includeContent.replace (start, lineEnd - start,
            "// begin of include from file " + filename + "\n" +
            content +
            "\n// end of included from file " + filename + "\n"
        );

        // go back to the beginning of the line to properly continue detecting things
        end = start;
    }

    end = 0;

    // finally, try to place the include contents before the main function
    while ((start = precompile.find (" main", end)) != std::string::npos) {
        char value = precompile.at(start + 5);

        end = start + 5;

        if (value != ' ' && value != '(') {
            continue;
        }

        // find the beginning of the line, inject include content and call it a day
        size_t previousLine = precompile.rfind ('\n', start);
        // finally insert it there
        precompile.insert (previousLine + 1, this->m_includeContent + '\n');
        // keep the iterator after the found function to prevent a loop
        end = end + this->m_includeContent.length () + 1;
    }

    // content should be ready, finally ask glslang to compile the shader
    this->m_compiledContent = CGLSLContext::get().toGlsl (precompile, this->m_type);
}

void CCompiler::parseComboConfiguration (const std::string& content, int defaultValue) {
    json data = json::parse (content);
    const auto combo = jsonFindRequired (data, "combo", "cannot parse combo information");
    const auto type = data.find ("type");
    const auto defvalue = data.find ("default");

    // check the combos
    const auto entry = this->m_combos->find (combo->get<std::string> ());

    // add the combo to the found list
    this->m_foundCombos->insert (std::make_pair<std::string, int> (*combo, true));

    // if the combo was not found in the predefined values this means that the default value in the JSON data can be
    // used so only define the ones that are not already defined
    if (entry == this->m_combos->end ()) {
        if (type != data.end ())
            sLog.error ("Resorting to default value as type ", *type, " is unknown");

        // if no combo is defined just load the default settings
        if (defvalue == data.end ()) {
            // TODO: PROPERLY SUPPORT EMPTY COMBOS
            this->m_combos->insert (std::make_pair<std::string, int> (*combo, (int) defaultValue));
        } else if (defvalue->is_number_float ()) {
            sLog.exception ("float combos are not supported in shader ", this->m_file, ". ", *combo);
        } else if (defvalue->is_number_integer ()) {
            this->m_combos->insert (std::make_pair<std::string, int> (*combo, defvalue->get<int> ()));
        } else if (defvalue->is_string ()) {
            sLog.exception ("string combos are not supported in shader ", this->m_file, ". ", *combo);
        } else {
            sLog.exception ("cannot parse combo information ", *combo, ". unknown type for ", defvalue->dump ());
        }
    }
}

void CCompiler::parseParameterConfiguration (const std::string& type, const std::string& name,
                                            const std::string& content) {
    json data = json::parse (content);
    const auto material = data.find ("material");
    const auto defvalue = data.find ("default");
    // auto range = data.find ("range");
    const auto combo = data.find ("combo");

    // this is not a real parameter
    auto constant = this->m_constants.end ();

    if (material != data.end ())
        constant = this->m_constants.find (*material);

    if (constant == this->m_constants.end () && defvalue == data.end ()) {
        if (type != "sampler2D")
            sLog.exception ("Cannot parse parameter data for ", name, " in shader ", this->m_file);
    }

    Variables::CShaderVariable* parameter = nullptr;

    // TODO: SUPPORT VALUES FOR ALL THESE TYPES
    if (type == "vec4") {
        parameter = new Variables::CShaderVariableVector4 (
            constant == this->m_constants.end () ? WallpaperEngine::Core::aToVector4 (*defvalue)
                                                 : *constant->second->as<CShaderConstantVector4> ()->getValue ());
    } else if (type == "vec3") {
        parameter = new Variables::CShaderVariableVector3 (
            constant == this->m_constants.end () ? WallpaperEngine::Core::aToVector3 (*defvalue)
                                                 : *constant->second->as<CShaderConstantVector4> ()->getValue ());
    } else if (type == "vec2") {
        parameter = new Variables::CShaderVariableVector2 (WallpaperEngine::Core::aToVector2 (*defvalue));
    } else if (type == "float") {
        float value = 0;

        if (constant == this->m_constants.end ())
            value = defvalue->get<float> ();
        else if (constant->second->is<CShaderConstantFloat> ())
            value = *constant->second->as<CShaderConstantFloat> ()->getValue ();
        else if (constant->second->is<CShaderConstantInteger> ())
            value = *constant->second->as<CShaderConstantInteger> ()->getValue ();

        parameter = new Variables::CShaderVariableFloat (value);
    } else if (type == "int") {
        int value = 0;

        if (constant == this->m_constants.end ())
            value = defvalue->get<int> ();
        else if (constant->second->is<CShaderConstantFloat> ())
            value = *constant->second->as<CShaderConstantFloat> ()->getValue ();
        else if (constant->second->is<CShaderConstantInteger> ())
            value = *constant->second->as<CShaderConstantInteger> ()->getValue ();

        parameter = new Variables::CShaderVariableInteger (value);
    } else if (type == "sampler2D" || type == "sampler2DComparison") {
        // samplers can have special requirements, check what sampler we're working with and create definitions
        // if needed
        const auto textureName = data.find ("default");
        // extract the texture number from the name
        const char value = name.at (std::string ("g_Texture").length ());
        // now convert it to integer
        size_t index = value - '0';

        if (combo != data.end ()) {
            // if the texture exists (and is not null), add to the combo
            if (this->m_passTextures.size () > index &&
                (!this->m_passTextures.at (index).empty () || textureName != data.end ())) {
                // add the new combo to the list
                this->m_combos->insert (std::make_pair<std::string, int> (*combo, 1));

                // textures linked to combos need to be tracked too
                if (this->m_foundCombos->find (*combo) == this->m_foundCombos->end ())
                    this->m_foundCombos->insert (std::make_pair<std::string, bool> (*combo, true));
            }
        }

        if (textureName != data.end ())
            this->m_textures.insert (std::make_pair (index, *textureName));

        // samplers are not saved, we can ignore them for now
        return;
    } else {
        sLog.error ("Unknown parameter type: ", type, " for ", name, " in shader ", this->m_file);
        return;
    }

    if (material != data.end ()) {
        parameter->setIdentifierName (*material);
        parameter->setName (name);

        this->m_parameters.push_back (parameter);
    }
}

Variables::CShaderVariable* CCompiler::findParameter (const std::string& identifier) {
    for (const auto& cur : this->m_parameters)
        if (cur->getIdentifierName () == identifier)
            return cur;

    return nullptr;
}

const std::vector<Variables::CShaderVariable*>& CCompiler::getParameters () const {
    return this->m_parameters;
}

std::map<std::string, int>* CCompiler::getCombos () const {
    return this->m_combos;
}

const std::map<int, std::string>& CCompiler::getTextures () const {
    return this->m_textures;
}
} // namespace WallpaperEngine::Render::Shaders
