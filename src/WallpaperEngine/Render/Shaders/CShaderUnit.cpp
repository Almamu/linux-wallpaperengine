#include "CShaderUnit.h"

#include <string>
#include <regex>
#include <stack>
#include <utility>
#include "WallpaperEngine/Logging/CLog.h"

#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector2.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector3.h"
#include "WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.h"

#include "CGLSLContext.h"
#include "WallpaperEngine/Assets/CAssetLoadException.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableFloat.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableInteger.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector2.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector3.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector4.h"

#define SHADER_HEADER(filename) "#version 330\n" \
    "// ======================================================\n" \
    "// Processed shader " + \
    filename + \
    "\n" \
    "// ======================================================\n" \
    "precision highp float;\n" \
    "#define mul(x, y) ((y) * (x))\n" \
    "#define max(x, y) max (y, x)\n" \
    "#define lerp mix\n" \
    "#define frac fract\n" \
    "#define CAST2(x) (vec2(x))\n" \
    "#define CAST3(x) (vec3(x))\n" \
    "#define CAST4(x) (vec4(x))\n" \
    "#define CAST3X3(x) (mat3(x))\n" \
    "#define saturate(x) (clamp(x, 0.0, 1.0))\n" \
    "#define texSample2D texture\n" \
    "#define texSample2DLod textureLod\n" \
    "#define log10(x) log2(x) * 0.301029995663981\n" \
    "#define atan2 atan\n" \
    "#define fmod(x, y) ((x)-(y)*trunc((x)/(y)))\n" \
    "#define ddx dFdx\n" \
    "#define ddy(x) dFdy(-(x))\n" \
    "#define GLSL 1\n\n";
#define FRAGMENT_SHADER_DEFINES "out vec4 out_FragColor;\n" \
    "#define varying in\n"
#define VERTEX_SHADER_DEFINES "#define attribute in\n" \
    "#define varying out\n"
#define DEFINE_COMBO(name, value) "#define " + name + " " + std::to_string (value) + "\n";

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Assets;
using namespace WallpaperEngine::Render::Shaders;

CShaderUnit::CShaderUnit (CGLSLContext::UnitType type, std::string file, std::string content,
    const CContainer* container, const std::map<std::string, const CShaderConstant*>& constants,
    const std::map<int, std::string>& passTextures, const std::map<std::string, int>& combos) :
    m_type (type),
    m_link (nullptr),
    m_container (container),
    m_file (std::move (file)),
    m_constants (constants),
    m_content (std::move(content)),
    m_passTextures (passTextures),
    m_combos (combos),
    m_discoveredCombos (),
    m_usedCombos () {
    // pre-process the shader so the units are clear
    this->preprocess ();
}

void CShaderUnit::preprocess () {
    this->m_preprocessed = this->m_content;
    this->m_includes = "";

    this->preprocessVariables ();
    this->preprocessIncludes ();
    this->preprocessRequires ();

    // replace gl_FragColor with the equivalent
    std::string from = "gl_FragColor";
    std::string to = "out_FragColor";

    size_t start_pos = 0;
    while ((start_pos = this->m_preprocessed.find (from, start_pos)) != std::string::npos) {
        this->m_preprocessed.replace (start_pos, from.length (), to);
        start_pos += to.length (); // Handles case where 'to' is a substring of 'from'
    }
}

void CShaderUnit::preprocessVariables () {
    this->m_preprocessed = this->m_content;
    this->m_includes = "";

    size_t start = 0, end = 0;
    while ((end = this->m_preprocessed.find ('\n', start)) != std::string::npos) {
        // Extract a line from the string
        std::string line = this->m_preprocessed.substr (start, end - start);
        size_t combo = line.find("// [COMBO] ");
        size_t uniform = line.find("uniform ");
        size_t comment = line.find("// ");
        size_t semicolon = line.find(';');

        if (combo != std::string::npos) {
            this->parseComboConfiguration (line.substr(combo + strlen("// [COMBO] ")), 0);
        } else if (
            uniform != std::string::npos &&
            comment != std::string::npos &&
            semicolon != std::string::npos &&
            // this check ensures that the comment is after the semicolon (so it's not a commented-out line)
            // this needs further refining as it's not taking into account block comments
            semicolon < comment) {
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
}

void CShaderUnit::preprocessIncludes () {
    size_t start = 0, end = 0;
    // prepare the include content
    while((start = this->m_preprocessed.find("#include", end)) != std::string::npos) {
        // TODO: CHECK FOR ERRORS HERE, MALFORMED INCLUDES WILL NOT BE PROPERLY HANDLED
        size_t quoteStart = this->m_preprocessed.find_first_of ('"', start) + 1;
        size_t quoteEnd = this->m_preprocessed.find_first_of('"', quoteStart);
        std::string filename = this->m_preprocessed.substr(quoteStart, quoteEnd - quoteStart);

        // some includes might not be present
        // and that should not be treated as an error mainly because these could come from
        // commented out content
        std::string content;

        try {
            content += "// begin of include from file ";
            content += filename;
            content += "\n";
            content += this->m_container->readIncludeShader (filename);
            content += "\n// end of included from file ";
            content += filename;
            content += "\n";
        } catch (CAssetLoadException&) {
            content += "// tried including file ";
            content += filename;
            content += " but was not found\n";
        }

        // replace the first two letters with a comment so the filelength doesn't change
        this->m_preprocessed = this->m_preprocessed.replace (start, 2, "//");

        this->m_includes += content;

        // go to the end of the line
        end = start;
    }

    // ensure the included files do not include other files
    end = 0;

    // then apply includes in-place
    while((start = this->m_includes.find("#include", end)) != std::string::npos) {
        size_t lineEnd = this->m_includes.find_first_of ('\n', start);
        // TODO: CHECK FOR ERRORS HERE, MALFORMED INCLUDES WILL NOT BE PROPERLY HANDLED
        size_t quoteStart = this->m_includes.find_first_of ('"', start) + 1;
        size_t quoteEnd = this->m_includes.find_first_of('"', quoteStart);
        std::string filename = this->m_includes.substr(quoteStart, quoteEnd - quoteStart);

        // some includes might not be present
        // and that should not be treated as an error mainly because these could come from
        // commented out content
        std::string content;

        try {
            content = "// begin of include from file ";
            content += filename;
            content += "\n";
            content += this->m_container->readIncludeShader (filename);
            content += "\n// end of included from file ";
            content += filename;
            content += "\n";
        } catch (CAssetLoadException&) {
            content = "// tried including file ";
            content += filename;
            content += " but was not found\n";
        }

        // file contents ready, replace things
        this->m_includes = this->m_includes.replace (start, lineEnd - start,content);

        // go back to the beginning of the line to properly continue detecting things
        end = start;
    }

    // search for the main function and add the includes before that for now
    end = 0;
    bool includesAdded = false;

    // finally, try to place the include contents before the main function
    while ((start = this->m_preprocessed.find (" main", end)) != std::string::npos) {
        char value = this->m_preprocessed.at(start + 5);

        end = start + 5;

        if (value != ' ' && value != '(') {
            continue;
        }

        // main located, search for uniforms and find the latest one available
        size_t lastAttribute = this->m_preprocessed.rfind ("attribute", start);
        size_t lastVarying = this->m_preprocessed.rfind ("varying", start);
        size_t lastUniform = this->m_preprocessed.rfind ("uniform", start);
        size_t latest = lastAttribute;

        if (latest == std::string::npos) {
            latest = lastVarying;
        } else if (latest < lastVarying && lastVarying != std::string::npos) {
            latest = lastVarying;
        }

        if (latest == std::string::npos) {
            latest = lastUniform;
        } else if (latest < lastUniform && lastUniform != std::string::npos) {
            latest = lastUniform;
        }

        if (latest < start) {
            // find the end of the current line
            latest = this->m_preprocessed.find ('\n', latest);
        } else {
            // find the end of the previous line
            latest = this->m_preprocessed.rfind ('\n', start);
        }

        // update the function start to point to the end of the previous line
        // as this will be used to determine the position of the includes
        start = this->m_preprocessed.rfind ('\n', start);

        // keeps track of the start and end of ifdefs to look for the right
        // place to put the includes in
        std::stack<size_t> ifdefStack;

        // start looking for #if and #endif results and add to the stack so we find the start of the current chain of ifdefs
        // and use that as point

        // for this we'll use regex
        std::regex ifdef (R"((#if|#endif))");
        std::smatch match;
        size_t current = 0;

        while (std::regex_search (this->m_preprocessed.cbegin () + current, this->m_preprocessed.cend (), match, ifdef)) {
            current += match.position ();

            // if it's opening an #ifdef keep track of the start of the block
            // and that's it
            if (this->m_preprocessed.substr (current, 3) == "#if") {
                // go to the next character so the regex doesn't match with the same thing again
                ifdefStack.push (current++);
                continue;
            }

            // go to the next character so the regex doesn't match with the same thing again
            current ++;

            // most likely a syntax error, but we'll ignore it for now...
            if (ifdefStack.empty ()) {
                continue;
            }

            size_t stackStart = ifdefStack.top ();
            ifdefStack.pop ();

            if (latest > stackStart && latest <= current) {
                latest = this->m_preprocessed.find ('\n', current);
            }
        }

        // no more matches, get the one that happens the earliest
        // TODO: IS THIS GOOD ENOUGH? MAYBE WE SHOULD BE GETTING THE FIRST #IF BLOCK INSTEAD?
        latest = std::min (latest, start);

        // finally insert it there
        this->m_preprocessed.insert (latest + 1, this->m_includes + '\n');
        includesAdded = true;
        break;
    }

    if (!includesAdded) {
        sLog.exception ("Could not find where to place includes for shader unit ", this->m_file);
    }
}

void CShaderUnit::preprocessRequires () {
    size_t start = 0, end = 0;
    // comment out requires
    while((start = this->m_preprocessed.find("#require", end)) != std::string::npos) {
        // TODO: CHECK FOR ERRORS HERE
        size_t lineEnd = this->m_preprocessed.find_first_of('\n', start);
        sLog.out("Shader has a require block ", this->m_preprocessed.substr (start, lineEnd - start));
        // replace the first two letters with a comment so the filelength doesn't change
        this->m_preprocessed = this->m_preprocessed.replace(start, 2, "//");

        // go to the end of the line
        end = lineEnd;
    }
}

void CShaderUnit::parseComboConfiguration (const std::string& content, int defaultValue) {
    // TODO: SUPPORT REQUIRES SO WE PROPERLY FOLLOW THE REQUIRED CHAIN
    json data = json::parse (content);
    const auto combo = jsonFindRequired (data, "combo", "cannot parse combo information");
    // ignore type as it seems to be used only on the editor
    // const auto type = data.find ("type");
    const auto defvalue = data.find ("default");

    // check the combos
    const auto entry = this->m_combos.find (combo->get<std::string> ());

    // add the combo to the found list
    this->m_usedCombos.insert (std::pair (*combo, true));

    // if the combo was not found in the predefined values this means that the default value in the JSON data can be
    // used so only define the ones that are not already defined
    if (entry == this->m_combos.end ()) {

        // if no combo is defined just load the default settings
        if (defvalue == data.end ()) {
            // TODO: PROPERLY SUPPORT EMPTY COMBOS
            this->m_discoveredCombos.insert (std::pair (*combo, (int) defaultValue));
        } else if (defvalue->is_number_float ()) {
            sLog.exception ("float combos are not supported in shader ", this->m_file, ". ", *combo);
        } else if (defvalue->is_number_integer ()) {
            this->m_discoveredCombos.insert (std::pair (*combo, defvalue->get<int> ()));
        } else if (defvalue->is_string ()) {
            sLog.exception ("string combos are not supported in shader ", this->m_file, ". ", *combo);
        } else {
            sLog.exception ("cannot parse combo information ", *combo, ". unknown type for ", defvalue->dump ());
        }
    }
}

void CShaderUnit::parseParameterConfiguration (
    const std::string& type, const std::string& name, const std::string& content
) {
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
    // TODO: MAYBE EVEN CONNECT THESE TO THE CORRESPONDING PROPERTY SO THINGS ARE UPDATED AS THE ORIGIN VALUES CHANGE?
    if (type == "vec4") {
        parameter = new Variables::CShaderVariableVector4(WallpaperEngine::Core::aToVector4 (*defvalue));
    } else if (type == "vec3") {
        parameter = new Variables::CShaderVariableVector3 (WallpaperEngine::Core::aToVector3 (*defvalue));
    } else if (type == "vec2") {
        parameter = new Variables::CShaderVariableVector2 (WallpaperEngine::Core::aToVector2 (*defvalue));
    } else if (type == "float") {
        if (defvalue->is_string ()) {
            parameter = new Variables::CShaderVariableFloat (strtof32 ((defvalue->get<std::string> ()).c_str (), nullptr));
        } else {
            parameter = new Variables::CShaderVariableFloat (*defvalue);
        }
    } else if (type == "int") {
        if (defvalue->is_string ()) {
            parameter = new Variables::CShaderVariableInteger (strtol((defvalue->get<std::string> ()).c_str (), nullptr, 10));
        } else {
            parameter = new Variables::CShaderVariableInteger (*defvalue);
        }
    } else if (type == "sampler2D" || type == "sampler2DComparison") {
        // samplers can have special requirements, check what sampler we're working with and create definitions
        // if needed
        const auto textureName = data.find ("default");
        // extract the texture number from the name
        const char value = name.at (std::string ("g_Texture").length ());
        const auto requireany = data.find ("requireany");
        const auto require = data.find ("require");
        // now convert it to integer
        size_t index = value - '0';

        if (combo != data.end ()) {
            // if the texture exists (and is not null), add to the combo
            auto texture = this->m_passTextures.find (index);
            bool isRequired = false;
            int comboValue = 1;

            if (texture != this->m_passTextures.end ()) {
                // nothing extra to do, the texture exists, the combo must be set
                // these tend to not have default value
                isRequired = true;
            } else if (require != data.end ()) {
                // this is required based on certain conditions
                if (requireany != data.end () && requireany->get <bool> ()) {

                    // any of the values set are valid, check for them
                    for (const auto& item : require->items ()) {
                        const std::string& macro = item.key ();
                        const auto it = this->m_combos.find (macro);

                        // if any of the values matched, this option is required
                        if (it == this->m_combos.end () || it->second != item.value ()) {
                            isRequired = true;
                            break;
                        }
                    }
                } else {
                    isRequired = true;

                    // all values must match for it to be required
                    for (const auto& item : require->items ()) {
                        const std::string& macro = item.key ();
                        const auto it = this->m_combos.find (macro);

                        // these can not exist and that'd be fine, we just care about the values
                        if (it != this->m_combos.end () && it->second == item.value ()) {
                            isRequired = false;
                            break;
                        }
                    }
                }
            }

            if (isRequired && texture == this->m_passTextures.end ()) {
                if (defvalue == data.end ()) {
                    isRequired = false;
                } else {
                    // is the combo registered already?
                    // if not, add it with the default value
                    const auto combo_it = this->m_combos.find (*combo);

                    // there's already a combo providing this value, so it doesn't need to be added
                    if (combo_it != this->m_combos.end ()) {
                        isRequired = false;
                        // otherwise a default value must be used
                    } else if (defvalue->is_string ()) {
                        comboValue = strtol (defvalue->get <std::string> ().c_str (), nullptr, 10);
                    } else if (defvalue->is_number()) {
                        comboValue = *defvalue;
                    } else {
                        sLog.exception ("Cannot determine default value for combo ", combo->get <std::string> (), " because it's not specified by the shader and is not given a default value: ", this->m_file);
                    }
                }
            }

            if (isRequired) {
                // add the new combo to the list
                this->m_discoveredCombos.insert (std::pair (*combo, comboValue));

                // textures linked to combos need to be tracked too
                if (this->m_usedCombos.find (*combo) == this->m_usedCombos.end ())
                    this->m_usedCombos.insert (std::pair (*combo, true));
            }
        }

        if (textureName != data.end ())
            this->m_defaultTextures.insert (std::pair (index, *textureName));

        // samplers are not saved, we can ignore them for now
        return;
    } else {
        sLog.error ("Unknown parameter type: ", type, " for ", name, " in shader ", this->m_file);
        return;
    }

    if (material != data.end () && parameter != nullptr) {
        parameter->setIdentifierName (*material);
        parameter->setName (name);

        this->m_parameters.push_back (parameter);
    }
}

const std::map<std::string, int>& CShaderUnit::getCombos () const {
    return this->m_combos;
}

const std::map<std::string, int>& CShaderUnit::getDiscoveredCombos () const {
    return this->m_discoveredCombos;
}

void CShaderUnit::linkToUnit (const CShaderUnit* unit) {
    this->m_link = unit;
}

const CShaderUnit* CShaderUnit::getLinkedUnit () const {
    return this->m_link;
}

const std::string& CShaderUnit::compile () {
    if (!this->m_final.empty ()) {
        return this->m_final;
    }

    this->m_final = SHADER_HEADER(this->m_file);

    if (this->m_type == CGLSLContext::UnitType_Fragment) {
        this->m_final += FRAGMENT_SHADER_DEFINES;
    } else {
        this->m_final += VERTEX_SHADER_DEFINES;
    }

    std::map<std::string, bool> addedCombos;

    // now add all the combos to the source
    for (const auto& combo : this->m_combos) {
        if (addedCombos.find (combo.first) == addedCombos.end ()) {
            this->m_final += DEFINE_COMBO (combo.first, combo.second);
        }
    }
    for (const auto& combo : this->m_discoveredCombos) {
        if (addedCombos.find (combo.first) == addedCombos.end ()) {
            this->m_final += DEFINE_COMBO (combo.first, combo.second);
        }
    }
    if (this->m_link != nullptr) {
        for (const auto& combo : this->m_link->getCombos ()) {
            if (addedCombos.find (combo.first) == addedCombos.end ()) {
                this->m_final += DEFINE_COMBO (combo.first, combo.second);
            }
        }
        for (const auto& combo : this->m_link->getDiscoveredCombos ()) {
            if (addedCombos.find (combo.first) == addedCombos.end ()) {
                this->m_final += DEFINE_COMBO (combo.first, combo.second);
            }
        }
    }

    // this should be the rest of the shader
    this->m_final += this->m_preprocessed;

    // shader compilation is handled by the pass itself, the unit doesn't have enough information for this step
    return this->m_final;
}

const std::vector<Variables::CShaderVariable*>& CShaderUnit::getParameters () const {
    return this->m_parameters;
}
const std::map<int, std::string>& CShaderUnit::getTextures () const {
    return this->m_defaultTextures;
}