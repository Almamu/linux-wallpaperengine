#include "common.h"
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

// filesystem
#include <WallpaperEngine/FileSystem/FileSystem.h>

// shader compiler
#include <WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h>
#include <WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h>
#include <WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.h>
#include <WallpaperEngine/Render/Shaders/Compiler.h>
#include <filesystem>
#include <regex>

#include "WallpaperEngine/Assets/CAssetLoadException.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableFloat.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableInteger.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector2.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector3.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector4.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Assets;

namespace WallpaperEngine::Render::Shaders {
Compiler::Compiler (CContainer* container, std::string filename, Type type, std::map<std::string, int>* combos,
                    std::map<std::string, bool>* foundCombos, const std::vector<std::string>& textures,
                    const std::map<std::string, CShaderConstant*>& constants, bool recursive) :
    m_combos (combos),
    m_foundCombos (foundCombos),
    m_passTextures (textures),
    m_recursive (recursive),
    m_type (type),
    m_file (std::move (filename)),
    m_error (),
    m_constants (constants),
    m_container (container) {
    if (type == Type_Vertex)
        this->m_content = this->m_container->readVertexShader (this->m_file);
    else if (type == Type_Pixel)
        this->m_content = this->m_container->readFragmentShader (this->m_file);
    else if (type == Type_Include)
        this->m_content = this->m_container->readIncludeShader (this->m_file);

    // clone the combos into the baseCombos to keep track of values that must be embedded no matter what
    for (const auto& [name, value] : *this->m_combos)
        this->m_baseCombos.insert (std::make_pair (name, value));
}

bool Compiler::peekString (std::string str, std::string::const_iterator& it) {
    std::string::const_iterator check = str.begin ();
    std::string::const_iterator cur = it;

    while (cur != this->m_content.end () && check != str.end ()) {
        if (*cur != *check)
            return false;

        ++cur;
        ++check;
    }

    if (cur == this->m_content.end ()) {
        return false;
    }

    if (check != str.end ()) {
        return false;
    }

    it = cur;

    return true;
}

bool Compiler::expectSemicolon (std::string::const_iterator& it) {
    if (*it != ';') {
        this->m_error = true;
        this->m_errorInfo = std::string ("Expected semicolon but got ") + *it;
        return false;
    }

    ++it;

    return true;
}

void Compiler::ignoreSpaces (std::string::const_iterator& it) {
    while (it != this->m_content.end () && (*it == ' ' || *it == '\t'))
        ++it;
}

void Compiler::ignoreUpToNextLineFeed (std::string::const_iterator& it) {
    while (it != this->m_content.end () && *it != '\n')
        ++it;
}

void Compiler::ignoreUpToBlockCommentEnd (std::string::const_iterator& it) {
    while (it != this->m_content.end () && !this->peekString ("*/", it))
        ++it;
}

std::string Compiler::extractType (std::string::const_iterator& it) {
    // first of all check for highp/mediump/lowp as these operators have to be ignored
    this->peekString ("highp", it);
    this->peekString ("mediump", it);
    this->peekString ("lowp", it);
    this->ignoreSpaces (it);

    auto cur = sTypes.begin ();
    const auto end = sTypes.end ();

    while (cur != end) {
        if (this->peekString (*cur + " ", it))
            return *cur;

        ++cur;
    }

    this->m_error = true;
    this->m_errorInfo = "Expected type";
    return "";
}

std::string Compiler::extractName (std::string::const_iterator& it) {
    std::string::const_iterator cur = it;
    std::string::const_iterator begin = cur;

    // first character has to be a valid alphabetic characer
    if (!this->isChar (cur) && *cur != '_') {
        this->m_error = true;
        this->m_errorInfo = "Expected name doesn't start with a valid character";
        return "";
    }

    ++cur;

    while (cur != this->m_content.end () && (this->isChar (cur) || *cur == '_' || this->isNumeric (cur)))
        ++cur;

    it = cur;

    return {begin, cur};
}

std::string Compiler::extractArray (std::string::const_iterator& it, bool mustExists) {
    auto cur = it;
    auto begin = cur;

    if (*cur != '[') {
        if (!mustExists) {
            return "";
        }

        this->m_error = true;
        this->m_errorInfo = "Expected an array but found nothing";
        return "";
    }

    ++cur;

    while (cur != this->m_content.end () && *cur != ']')
        ++cur;

    it = ++cur;

    return {begin, cur};
}

bool Compiler::isChar (const std::string::const_iterator& it) {
    return ((*it) >= 'A' && (*it) <= 'Z') || ((*it) >= 'a' && (*it) <= 'z');
}

bool Compiler::isNumeric (const std::string::const_iterator& it) {
    return (*it) >= '0' && (*it) <= '9';
}

std::string Compiler::extractQuotedValue (std::string::const_iterator& it) {
    std::string::const_iterator cur = it;

    if (*cur != '"') {
        m_error = true;
        m_errorInfo = std::string ("Expected opening \" but got ") + (*cur);
        return "";
    }

    ++cur;

    while (cur != this->m_content.end () && *cur != '\n' && *cur != '"')
        ++cur;

    if (cur == this->m_content.end ()) {
        m_error = true;
        m_errorInfo = "Expected closing \" not found";
        it = cur;
        return "";
    }

    std::string filename = std::string (++it, cur);

    it = ++cur;

    return filename;
}

std::string Compiler::lookupShaderFile (std::string filename) {
    // now compile the new shader
    // do not include the default header (as it's already included in the parent)
    Compiler loader (this->m_container, std::move (filename), Type_Include, this->m_combos, this->m_foundCombos,
                     this->m_passTextures, this->m_constants, true);

    loader.precompile ();

    return loader.getCompiled ();
}

std::string& Compiler::getCompiled () {
    return this->m_compiledContent;
}

void Compiler::precompile () {
    // TODO: SEPARATE THIS IN TWO SO THE COMBOS ARE DETECTED FIRST AND WE DO NOT REQUIRE DOUBLE COMPILATION OF THE
    // SHADER'S SOURCE
#define BREAK_IF_ERROR                                                                                                 \
    if (this->m_error) {                                                                                               \
        sLog.exception ("ERROR PRE-COMPILING SHADER.", this->m_errorInfo);                                             \
    }
    // parse the shader and find #includes and such things and translate them to the correct name
    // also remove any #version definition to prevent errors
    std::string::const_iterator it = this->m_content.begin ();

    // reset error indicator
    this->m_error = false;
    this->m_errorInfo = "";
    this->m_compiledContent = "";
    this->m_includesContent = "";
    this->m_includesProcessed = false;

    // search preprocessor macros and parse them
    while (it != this->m_content.end () && !this->m_error) {
        if (*it == ' ' || *it == '\t' || *it == '\n' || *it == '\r' || *it == '\0' || *it == '{' || *it == '}' ||
            *it == '[' || *it == ']' || *it == '.') {
            this->m_compiledContent += *it;
            ++it;
        } else if (*it == '#') {
            if (this->peekString ("#include ", it)) {
                // ignore whitespaces
                this->ignoreSpaces (it);
                BREAK_IF_ERROR
                // extract value between quotes
                std::string filename = this->extractQuotedValue (it);
                BREAK_IF_ERROR

                if (this->m_recursive) {
                    this->m_compiledContent += "// begin of include from file " + filename + "\r\n";
                    this->m_compiledContent += this->lookupShaderFile (filename);
                    this->m_compiledContent += "\r\n// end of included from file " + filename + "\r\n";
                } else {
                    // load the content to the includes contents and continue with the next one
                    // try to find the file first
                    this->m_includesContent += "// begin of included from file " + filename + "\r\n";
                    this->m_includesContent += this->lookupShaderFile (filename);
                    this->m_includesContent += "\r\n// end of included from file " + filename + "\r\n";
                }
            } else {
                this->m_compiledContent += '#';
                ++it;
            }
        } else if (*it == 'u') {
            // uniforms might have extra information for their values
            if (this->peekString ("uniform ", it)) {
                this->ignoreSpaces (it);
                std::string type = this->extractType (it);
                BREAK_IF_ERROR
                this->ignoreSpaces (it);
                std::string name = this->extractName (it);
                BREAK_IF_ERROR
                this->ignoreSpaces (it);
                std::string array = this->extractArray (it, false);
                BREAK_IF_ERROR
                this->ignoreSpaces (it);
                this->expectSemicolon (it);
                BREAK_IF_ERROR
                this->ignoreSpaces (it);

                // check if there is any actual extra information and parse it
                if (this->peekString ("//", it)) {
                    this->ignoreSpaces (it);
                    std::string::const_iterator begin = it;
                    this->ignoreUpToNextLineFeed (it);

                    std::string configuration;
                    configuration.append (begin, it);

                    // parse the parameter information
                    this->parseParameterConfiguration (type, name, configuration);
                    BREAK_IF_ERROR
                    this->m_compiledContent += "uniform ";
                    this->m_compiledContent += type;
                    this->m_compiledContent += " ";
                    this->m_compiledContent += name;
                    this->m_compiledContent += array;
                    this->m_compiledContent += "; // ";
                    this->m_compiledContent += configuration;
                } else {
                    this->m_compiledContent += "uniform ";
                    this->m_compiledContent += type;
                    this->m_compiledContent += " ";
                    this->m_compiledContent += name;
                    this->m_compiledContent += array;
                    this->m_compiledContent += ";";
                }
            } else {
                // continue reading the original shader as this most likely is a variable name or something
                // that is not really interesting for the compiler
                this->m_compiledContent += *it;
                ++it;
            }
        } else if (*it == 'a') {
            // find attribute definitions
            if (this->peekString ("attribute ", it)) {
                this->ignoreSpaces (it);
                std::string type = this->extractType (it);
                BREAK_IF_ERROR
                this->ignoreSpaces (it);
                std::string name = this->extractName (it);
                BREAK_IF_ERROR
                this->ignoreSpaces (it);
                std::string array = this->extractArray (it, false);
                BREAK_IF_ERROR
                this->ignoreSpaces (it);
                this->expectSemicolon (it);
                BREAK_IF_ERROR

                this->m_compiledContent += "attribute " + type + " " + name + array + ";";
            } else {
                // check for types first
                std::string type = this->extractType (it);

                // types not found, try names
                if (!this->m_error) {
                    this->ignoreSpaces (it);
                    this->m_compiledContent += type;
                } else {
                    this->m_error = false;
                    std::string name = this->extractName (it);

                    if (!this->m_error) {
                        // check if the name is a translated one or not
                        this->m_compiledContent += name;
                    } else {
                        this->m_error = false;
                        this->m_compiledContent += *it;
                        ++it;
                    }
                }
            }
        } else if (*it == '/') {
            if (this->peekString ("//", it)) {
                std::string::const_iterator begin = it - 2;
                // is there a COMBO mark to take care of?
                this->ignoreSpaces (it);

                if (this->peekString ("[COMBO]", it)) {
                    // parse combo json data to define the proper variables
                    this->ignoreSpaces (it);
                    begin = it;
                    this->ignoreUpToNextLineFeed (it);

                    std::string configuration;
                    configuration.append (begin, it);

                    this->m_compiledContent += "// [COMBO] " + configuration;

                    this->parseComboConfiguration (configuration, 0);
                    BREAK_IF_ERROR;
                } else if (this->peekString ("[COMBO_OFF]", it)) {
                    // parse combo json data to define the proper variables
                    this->ignoreSpaces (it);
                    begin = it;
                    this->ignoreUpToNextLineFeed (it);

                    std::string configuration;
                    configuration.append (begin, it);

                    this->m_compiledContent += "// [COMBO_OFF] " + configuration;

                    this->parseComboConfiguration (configuration, 0);
                    BREAK_IF_ERROR;
                } else {
                    // the comment can be ignored and put back as is
                    this->ignoreUpToNextLineFeed (it);
                    this->m_compiledContent.append (begin, it);
                }
            } else if (this->peekString ("/*", it)) {
                std::string::const_iterator begin = it - 2;
                this->ignoreUpToBlockCommentEnd (it);
                this->m_compiledContent.append (begin, it);
            } else {
                this->m_compiledContent += *it;
                ++it;
            }
        } else {
            // check for types first
            std::string type = this->extractType (it);

            // type found
            if (!this->m_error) {
                this->ignoreSpaces (it);
                // check for main, and take it into account, this also helps adding the includes
                std::string name = this->extractName (it);

                this->ignoreSpaces (it);

                if (this->peekString ("(", it)) {
                    if (!this->m_includesProcessed) {
                        this->m_compiledContent += "\n\n" + this->m_includesContent + "\n\n";
                        this->m_includesProcessed = true;
                    }

                    this->m_compiledContent += type + " " + name + "(";
                } else {
                    this->m_compiledContent += type + " " + name;
                }
            } else {
                this->m_error = false;
                std::string name = this->extractName (it);

                if (!this->m_error) {
                    // check if the name is a translated one or not
                    this->m_compiledContent += name;
                } else {
                    this->m_error = false;
                    this->m_compiledContent += *it++;
                }
            }
        }
    }

    std::string finalCode;

    if (!this->m_recursive) {
        // add the opengl compatibility at the top
        finalCode = "#version 330\n"
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

        if (this->m_type == Type_Vertex) {
            finalCode += "#define attribute in\n"
                         "#define varying out\n";
        } else {
            finalCode += "out vec4 out_FragColor;\n"
                         "#define varying in\n";
        }

        finalCode += "// ======================================================\n"
                     "// Shader combo parameter definitions\n"
                     "// ======================================================\n";

        finalCode += "// found combos from current shader\n";

        // add combo values
        for (const auto& [name, value] : *this->m_foundCombos) {
            // find the right value for the combo in the combos map
            auto combo = this->m_combos->find (name);

            if (combo == this->m_combos->end ())
                continue;

            finalCode += "#define " + name + " " + std::to_string (combo->second) + "\n";
        }

        finalCode += "// combos from pass\n";

        // add base combos that come from the pass change that MUST be added
        for (const auto& [name, value] : this->m_baseCombos) {
            auto alreadyFound = this->m_foundCombos->find (name);

            if (alreadyFound != this->m_foundCombos->end ())
                continue;

            finalCode += "#define " + name + " " + std::to_string (value) + "\n";
        }
    }

    // replace gl_FragColor with the equivalent
    std::string from = "gl_FragColor";
    std::string to = "out_FragColor";

    size_t start_pos = 0;
    while ((start_pos = this->m_compiledContent.find (from, start_pos)) != std::string::npos) {
        this->m_compiledContent.replace (start_pos, from.length (), to);
        start_pos += to.length (); // Handles case where 'to' is a substring of 'from'
    }

    // replace sample occurrences
    from = "sample";
    to = "_sample";

    start_pos = 0;
    while ((start_pos = this->m_compiledContent.find (from, start_pos)) != std::string::npos) {
        // ensure that after it comes something like a space or a ; or a tab
        std::string after = this->m_compiledContent.substr (start_pos + from.length (), 1);

        if (after != " " && after != ";" && after != "\t" && after != "=" && after != "+" && after != "-" &&
            after != "/" && after != "*" && after != "." && after != "," && after != ")") {
            start_pos += to.length (); // Handles case where 'to' is a substring of 'from'
            continue;
        }

        this->m_compiledContent.replace (start_pos, from.length (), to);
        start_pos += to.length (); // Handles case where 'to' is a substring of 'from'
    }

    try {
        this->applyPatches ();
    } catch (CAssetLoadException&) {
        // nothing important, no patch was found
    }

    finalCode += this->m_compiledContent;

    if (!this->m_recursive) {
        sLog.debug ("======================== COMPILED ", (this->m_type == Type_Vertex ? "VERTEX" : "FRAGMENT"),
                    " SHADER ", this->m_file, " ========================");
        sLog.debug (finalCode);
    }

    // store the final final code here
    this->m_compiledContent = finalCode;
#undef BREAK_IF_ERROR
}

void Compiler::applyPatches () {
    // small patches for things, looks like the official wpengine does the same thing
    std::filesystem::path file = this->m_file;
    file = "patches" / file.filename ();

    if (this->m_type == Type_Vertex)
        file += ".vert";
    else if (this->m_type == Type_Pixel)
        file += ".frag";

    file += ".json";

    std::string tmp = file;
    const std::string patchContents = this->m_container->readFileAsString (file);

    json data = json::parse (patchContents);
    const auto patches = data.find ("patches");

    for (auto patch : *patches) {
        auto matches = patch.find ("matches");
        bool canApply = true;

        // check for matches first, as these signal whether the patch can be applied or not
        for (const auto& match : *matches) {
            if (this->m_compiledContent.find (match) == std::string::npos) {
                canApply = false;
                break;
            }
        }

        if (canApply == false)
            continue;

        const auto replacements = patch.find ("replacements");

        for (const auto& replacement : replacements->items ()) {
            // replace gl_FragColor with the equivalent
            std::string from = replacement.key ();
            std::string to = replacement.value ();

            size_t start_pos = 0;
            while ((start_pos = this->m_compiledContent.find (from, start_pos)) != std::string::npos) {
                this->m_compiledContent.replace (start_pos, from.length (), to);
                start_pos += to.length (); // Handles case where 'to' is a substring of 'from'
            }
        }
    }
}

void Compiler::parseComboConfiguration (const std::string& content, int defaultValue) {
    json data = json::parse (content);
    const auto combo = jsonFindRequired (data, "combo", "cannot parse combo information");
    const auto type = data.find ("type");
    const auto defvalue = data.find ("default");

    // add line feed just in case
    this->m_compiledContent += "\n";

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

void Compiler::parseParameterConfiguration (const std::string& type, const std::string& name,
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

    Variables::CShaderVariable* parameter;

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
    } else if (type == "sampler2D") {
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
        this->m_error = true;
        this->m_errorInfo = "Unknown parameter type: " + type + " for " + name;
        return;
    }

    if (material != data.end ()) {
        parameter->setIdentifierName (*material);
        parameter->setName (name);

        this->m_parameters.push_back (parameter);
    }
}

Variables::CShaderVariable* Compiler::findParameter (const std::string& identifier) {
    for (const auto& cur : this->m_parameters)
        if (cur->getIdentifierName () == identifier)
            return cur;

    return nullptr;
}

const std::vector<Variables::CShaderVariable*>& Compiler::getParameters () const {
    return this->m_parameters;
}

std::map<std::string, int>* Compiler::getCombos () const {
    return this->m_combos;
}

const std::map<int, std::string>& Compiler::getTextures () const {
    return this->m_textures;
}

std::vector<std::string> Compiler::sTypes = {
    "vec4",  "uvec4", "ivec4", "dvec4", "bvec4", "vec3",      "uvec3",  "ivec3", "dvec3", "bvec3", "vec2",
    "uvec2", "ivec2", "dvec2", "bvec2", "float", "sampler2D", "mat4x3", "mat4",  "mat3",  "uint4", "void"};
} // namespace WallpaperEngine::Render::Shaders
