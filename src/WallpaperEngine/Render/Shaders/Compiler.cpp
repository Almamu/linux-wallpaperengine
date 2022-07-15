#include <iostream>
#include <fstream>
#include <string>
#include <utility>

// filesystem
#include <WallpaperEngine/FileSystem/FileSystem.h>

// shader compiler
#include <WallpaperEngine/Render/Shaders/Compiler.h>
#include <WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.h>
#include <WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h>
#include <WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableFloat.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableInteger.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector2.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector3.h"
#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariableVector4.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Assets;

namespace WallpaperEngine::Render::Shaders
{
    Compiler::Compiler (
            CContainer* container,
            std::string filename,
            Type type,
            std::map<std::string, int>* combos,
            const std::map<std::string, CShaderConstant*>& constants,
            bool recursive) :
        m_combos (combos),
        m_recursive (recursive),
        m_type (type),
        m_file (std::move(filename)),
        m_error (""),
        m_errorInfo (""),
        m_constants (constants),
        m_container (container)
    {
        if (type == Type_Vertex)
            this->m_content = this->m_container->readVertexShader (this->m_file);
        else if (type == Type_Pixel)
            this->m_content = this->m_container->readFragmentShader (this->m_file);
        else if (type == Type_Include)
            this->m_content = this->m_container->readIncludeShader (this->m_file);
    }

    bool Compiler::peekString(std::string str, std::string::const_iterator& it)
    {
        std::string::const_iterator check = str.begin();
        std::string::const_iterator cur = it;

        while (cur != this->m_content.end () && check != str.end ())
        {
            if (*cur != *check) return false;

            cur ++; check ++;
        }

        if (cur == this->m_content.end ())
        {
            return false;
        }

        if (check != str.end ())
        {
            return false;
        }

        it = cur;

        return true;
    }

    bool Compiler::expectSemicolon (std::string::const_iterator& it)
    {
        if (*it != ';')
        {
            this->m_error = true;
            this->m_errorInfo = "Expected semicolon but got " + *it;
            return false;
        }

        it ++;

        return true;
    }

    void Compiler::ignoreSpaces(std::string::const_iterator &it)
    {
        while (it != this->m_content.end() && (*it == ' ' || *it == '\t')) it ++;
    }

    void Compiler::ignoreUpToNextLineFeed (std::string::const_iterator& it)
    {
        while (it != this->m_content.end() && *it != '\n') it ++;
    }

    void Compiler::ignoreUpToBlockCommentEnd (std::string::const_iterator& it)
    {
        while (it != this->m_content.end() && this->peekString ("*/", it) == false) it ++;
    }

    std::string Compiler::extractType (std::string::const_iterator& it)
    {
        // first of all check for highp/mediump/lowp as these operators have to be ignored
        this->peekString ("highp", it);
        this->peekString ("mediump", it);
        this->peekString ("lowp", it);
        this->ignoreSpaces (it);

        auto cur = sTypes.begin ();
        auto end = sTypes.end ();

        while (cur != end)
        {
            if (this->peekString (*cur + " ", it) == true)
            {
                return *cur;
            }

            cur ++;
        }

        this->m_error = true;
        this->m_errorInfo = "Expected type";
        return "";
    }

    std::string Compiler::extractName (std::string::const_iterator& it)
    {
        std::string::const_iterator cur = it;
        std::string::const_iterator begin = cur;

        // first character has to be a valid alphabetic characer
        if (this->isChar (cur) == false && *cur != '_')
        {
            this->m_error = true;
            this->m_errorInfo = "Expected name doesn't start with a valid character";
            return "";
        }

        cur ++;

        while (cur != this->m_content.end () && (this->isChar (cur) == true || *cur == '_' || this->isNumeric (cur) == true)) cur ++;

        it = cur;

        return std::string (begin, cur);
    }

    std::string Compiler::extractArray(std::string::const_iterator &it, bool mustExists)
    {
        std::string::const_iterator cur = it;
        std::string::const_iterator begin = cur;

        if (*cur != '[')
        {
            if (mustExists == false)
                return "";

            this->m_error = true;
            this->m_errorInfo = "Expected an array but found nothing";
            return "";
        }

        cur ++;

        while (cur != this->m_content.end () && *cur != ']') cur ++;

        it = ++cur;

        return std::string (begin, cur);
    }

    bool Compiler::isChar (std::string::const_iterator& it)
    {
        return ((*it) >= 'A' && (*it) <= 'Z') || ((*it) >= 'a' && (*it) <= 'z');
    }

    bool Compiler::isNumeric (std::string::const_iterator& it)
    {
        return (*it) >= '0' && (*it) <= '9';
    }

    std::string Compiler::extractQuotedValue(std::string::const_iterator& it)
    {
        std::string::const_iterator cur = it;

        if (*cur != '"')
        {
            m_error = true;
            m_errorInfo = "Expected opening \" but got " + (*cur);
            return "";
        }

        cur ++;

        while (cur != this->m_content.end () && *cur != '\n' && *cur != '"') cur ++;

        if (cur == this->m_content.end ())
        {
            m_error = true;
            m_errorInfo = "Expected closing \" not found";
            it = cur;
            return "";
        }

        std::string filename = std::string (++it, cur);

        it = ++cur;

        return filename;
    }

    std::string Compiler::lookupShaderFile (std::string filename)
    {
        // now compile the new shader
        // do not include the default header (as it's already included in the parent)
        Compiler loader (this->m_container, std::move (filename), Type_Include, this->m_combos, this->m_constants, true);

        loader.precompile ();

        return loader.getCompiled ();
    }

    std::string& Compiler::getCompiled ()
    {
        return this->m_compiledContent;
    }

    void Compiler::precompile()
    {
        // TODO: SEPARATE THIS IN TWO SO THE COMBOS ARE DETECTED FIRST AND WE DO NOT REQUIRE DOUBLE COMPILATION OF THE SHADER'S SOURCE
    #define BREAK_IF_ERROR if (this->m_error == true) { throw std::runtime_error ("ERROR PRE-COMPILING SHADER" + this->m_errorInfo); }
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
        while (it != this->m_content.end () && this->m_error == false)
        {
            if (*it == ' ' || *it == '\t' || *it == '\n' || *it == '\r' || *it == '\0' || *it == '{' || *it == '}' || *it == '[' || *it == ']' || *it == '.')
            {
                this->m_compiledContent += *it;
                it ++;
            }
            else if (*it == '#')
            {
                if (this->peekString ("#include ", it) == true)
                {
                    std::string filename = "";

                    // ignore whitespaces
                    this->ignoreSpaces (it); BREAK_IF_ERROR
                    // extract value between quotes
                    filename = this->extractQuotedValue (it); BREAK_IF_ERROR

                    if (this->m_recursive)
                    {
                        this->m_compiledContent += "// begin of include from file " + filename + "\r\n";
                        this->m_compiledContent += this->lookupShaderFile (filename);
                        this->m_compiledContent += "\r\n// end of included from file " + filename + "\r\n";
                    }
                    else
                    {
                        // load the content to the includes contents and continue with the next one
                        // try to find the file first
                        this->m_includesContent += "// begin of included from file " + filename + "\r\n";
                        this->m_includesContent += this->lookupShaderFile (filename);
                        this->m_includesContent += "\r\n// end of included from file " + filename + "\r\n";
                    }
                }
                else
                {
                    this->m_compiledContent += '#';
                    it ++;
                }
            }
            else if (*it == 'u')
            {
                // uniforms might have extra information for their values
                if (this->peekString ("uniform ", it) == true)
                {
                    this->ignoreSpaces (it);
                    std::string type = this->extractType (it); BREAK_IF_ERROR
                    this->ignoreSpaces (it);
                    std::string name = this->extractName (it); BREAK_IF_ERROR
                    this->ignoreSpaces (it);
                    std::string array = this->extractArray (it, false); BREAK_IF_ERROR
                    this->ignoreSpaces (it);
                    this->expectSemicolon (it); BREAK_IF_ERROR
                    this->ignoreSpaces (it);

                    // check if there is any actual extra information and parse it
                    if (this->peekString ("//", it) == true)
                    {
                        this->ignoreSpaces (it);
                        std::string::const_iterator begin = it;
                        this->ignoreUpToNextLineFeed (it);

                        std::string configuration; configuration.append (begin, it);

                        // parse the parameter information
                        this->parseParameterConfiguration (type, name, configuration); BREAK_IF_ERROR
                        this->m_compiledContent += "uniform " + type + " " + name + array + "; // " + configuration;
                    }
                    else
                    {
                        this->m_compiledContent += "uniform " + type + " " + name + array + ";";
                    }
                }
                else
                {
                    // continue reading the original shader as this most likely is a variable name or something
                    // that is not really interesting for the compiler
                    this->m_compiledContent += *it;
                    it ++;
                }
            }
            else if (*it == 'a')
            {
                // find attribute definitions
                if (this->peekString ("attribute ", it) == true)
                {
                    this->ignoreSpaces (it);
                    std::string type = this->extractType (it); BREAK_IF_ERROR
                    this->ignoreSpaces (it);
                    std::string name = this->extractName (it); BREAK_IF_ERROR
                    this->ignoreSpaces (it);
                    std::string array = this->extractArray (it, false); BREAK_IF_ERROR
                    this->ignoreSpaces (it);
                    this->expectSemicolon (it); BREAK_IF_ERROR

                    this->m_compiledContent += "attribute " + type + " " + name + array + ";";
                }
                else
                {
                    // check for types first
                    std::string type = this->extractType (it);

                    // types not found, try names
                    if (this->m_error == false)
                    {
                        this->m_compiledContent += type;
                    }
                    else
                    {
                        this->m_error = false;
                        std::string name = this->extractName (it);

                        if (this->m_error == false)
                        {
                            // check if the name is a translated one or not
                            this->m_compiledContent += name;
                        }
                        else
                        {
                            this->m_error = false;
                            this->m_compiledContent += *it;
                            it ++;
                        }
                    }
                }
            }
            else if (*it == '/')
            {
                if (this->peekString ("//", it) == true)
                {
                    std::string::const_iterator begin = it - 2;
                    // is there a COMBO mark to take care of?
                    this->ignoreSpaces (it);

                    if (this->peekString ("[COMBO]", it) == true)
                    {
                        // parse combo json data to define the proper variables
                        this->ignoreSpaces (it);
                        begin = it;
                        this->ignoreUpToNextLineFeed (it);

                        std::string configuration; configuration.append (begin, it);

                        this->m_compiledContent += "// [COMBO] " + configuration;

                        this->parseComboConfiguration (configuration, 1); BREAK_IF_ERROR;
                    }
                    else if (this->peekString ("[COMBO_OFF]", it) == true)
                    {
                        // parse combo json data to define the proper variables
                        this->ignoreSpaces (it);
                        begin = it;
                        this->ignoreUpToNextLineFeed (it);

                        std::string configuration; configuration.append (begin, it);

                        this->m_compiledContent += "// [COMBO_OFF] " + configuration;

                        this->parseComboConfiguration (configuration, 0); BREAK_IF_ERROR;
                    }
                    else
                    {
                        // the comment can be ignored and put back as is
                        this->ignoreUpToNextLineFeed(it);
                        this->m_compiledContent.append (begin, it);
                    }
                }
                else if (this->peekString ("/*", it) == true)
                {
                    std::string::const_iterator begin = it - 2;
                    this->ignoreUpToBlockCommentEnd (it);
                    this->m_compiledContent.append (begin, it);
                }
                else
                {
                    this->m_compiledContent += *it;
                    it ++;
                }
            }
            else
            {
                // check for types first
                std::string type = this->extractType (it);

                // types not found, try names
                if (this->m_error == false)
                {
                    // check for main, and take it into account, this also helps adding the includes
                    std::string name = this->extractName (it);

                    this->ignoreSpaces (it);

                    if (this->peekString ("(", it) == true)
                    {
                        if (this->m_includesProcessed == false)
                        {
                            this->m_compiledContent += "\n\n" + this->m_includesContent + "\n\n";
                            this->m_includesProcessed = true;
                        }

                        this->m_compiledContent += type + " " + name + "(";
                    }
                    else
                    {
                        this->m_compiledContent += type + " " + name;
                    }
                }
                else
                {
                    this->m_error = false;
                    std::string name = this->extractName (it);

                    if (this->m_error == false)
                    {
                        // check if the name is a translated one or not
                        this->m_compiledContent += name;
                    }
                    else
                    {
                        this->m_error = false;
                        this->m_compiledContent += *it++;
                    }
                }
            }
        }

        std::string finalCode;

        if (this->m_recursive == false)
        {
            // add the opengl compatibility at the top
            finalCode =   "#version 150\n"
                          "#define highp\n"
                          "#define mediump\n"
                          "#define lowp\n"
                          "#define mul(x, y) ((y) * (x))\n"
                          "#define max(x, y) max (y, x)\n"
                          "#define lerp mix\n"
                          "#define frac fract\n"
                          "#define CAST2(x) (vec2(x))\n"
                          "#define CAST3(x) (vec3(x))\n"
                          "#define CAST4(x) (vec4(x))\n"
                          "#define CAST3X3(x) (mat3(x))\n"
                          "#define saturate(x) (clamp(x, 0.0, 1.0))\n"
                          "#define texSample2D texture2D\n"
                          "#define texSample2DLod textureLod\n"
                          "#define atan2 atan\n"
                          "#define ddx dFdx\n"
                          "#define ddy(x) dFdy(-(x))\n"
                          "#define GLSL 1\n"
                          "#define HLSL 1\n"
                          "#define float1 float\n"
                          "#define float2 vec2\n"
                          "#define float3 vec3\n"
                          "#define float4 vec4\n";

            if (this->m_type == Type_Vertex)
            {
                finalCode +=
                    "#define varying out\n"
                    "#define attribute in\n";
            }
            else
            {
                finalCode +=
                    "#define varying in\n"
                    "#define gl_FragColor glOutColor\n"
                    "out vec4 glOutColor;\n";
            }

            // add combo values
            auto cur = this->m_combos->begin ();
            auto end = this->m_combos->end ();

            for (; cur != end; cur ++)
            {
                finalCode += "#define " + (*cur).first + " " + std::to_string ((*cur).second) + "\n";
            }
        }

        finalCode += this->m_compiledContent;

        if (DEBUG && this->m_recursive == false)
        {
            if (this->m_type == Type_Vertex)
                std::cout << "======================== COMPILED VERTEX SHADER " << this->m_file.c_str () << " ========================" << std::endl;
            else
                std::cout << "======================== COMPILED FRAGMENT SHADER " << this->m_file.c_str () << " ========================" << std::endl;
            std::cout << finalCode << std::endl;
        }

        // store the final final code here
        this->m_compiledContent = finalCode;
    #undef BREAK_IF_ERROR
    }

    void Compiler::parseComboConfiguration (const std::string& content, int defaultValue)
    {
        json data = json::parse (content);
        auto combo = jsonFindRequired (data, "combo", "cannot parse combo information");
        auto defvalue = data.find ("default");

        // add line feed just in case
        this->m_compiledContent += "\n";

        // check the combos
        std::map<std::string, int>::const_iterator entry = this->m_combos->find ((*combo).get <std::string> ());

        // if the combo was not found in the predefined values this means that the default value in the JSON data can be used
        // so only define the ones that are not already defined
        if (entry == this->m_combos->end ())
        {
            // if no combo is defined just load the default settings
            if (defvalue == data.end ())
            {
                // TODO: PROPERLY SUPPORT EMPTY COMBOS
                this->m_combos->insert (std::make_pair <std::string, int> (*combo, (int) defaultValue));
            }
            else if ((*defvalue).is_number_float ())
            {
                throw std::runtime_error ("float combos not supported");
            }
            else if ((*defvalue).is_number_integer ())
            {
                this->m_combos->insert (std::make_pair <std::string, int> (*combo, (*defvalue).get <int> ()));
            }
            else if ((*defvalue).is_string ())
            {
                throw std::runtime_error ("string combos not supported");
            }
            else
            {
                throw std::runtime_error ("cannot parse combo information, unknown type");
            }
        }
    }

    void Compiler::parseParameterConfiguration (const std::string& type, const std::string& name, const std::string& content)
    {
        json data = json::parse (content);
        auto material = data.find ("material");
        auto defvalue = data.find ("default");
        auto range = data.find ("range");
        auto combo = data.find ("combo");

        // this is not a real parameter
        auto constant = this->m_constants.end ();

        if (material != data.end ())
            constant = this->m_constants.find (*material);

        if (constant == this->m_constants.end () && defvalue == data.end ())
        {
            if (type != "sampler2D")
                throw std::runtime_error ("cannot parse parameter data");
        }

        Variables::CShaderVariable* parameter = nullptr;

        // TODO: SUPPORT VALUES FOR ALL THESE TYPES
        if (type == "vec4")
        {
            parameter = new Variables::CShaderVariableVector4 (
                constant == this->m_constants.end ()
                ? WallpaperEngine::Core::aToVector4 (*defvalue)
                : *(*constant).second->as <CShaderConstantVector4> ()->getValue ()
            );
        }
        else if (type == "vec3")
        {
            parameter = new Variables::CShaderVariableVector3 (
                constant == this->m_constants.end ()
                ? WallpaperEngine::Core::aToVector3 (*defvalue)
                : *(*constant).second->as <CShaderConstantVector4> ()->getValue ()
            );
        }
        else if (type == "vec2")
        {
            parameter = new Variables::CShaderVariableVector2 (
                WallpaperEngine::Core::aToVector2 (*defvalue)
            );
        }
        else if (type == "float")
        {
            float value = 0;

            if (constant == this->m_constants.end ())
                value = (*defvalue).get <float> ();
            else if ((*constant).second->is <CShaderConstantFloat> () == true)
                value = *(*constant).second->as <CShaderConstantFloat> ()->getValue ();
            else if ((*constant).second->is <CShaderConstantInteger> () == true)
                value = *(*constant).second->as <CShaderConstantInteger> ()->getValue ();

            parameter = new Variables::CShaderVariableFloat (value);
        }
        else if (type == "int")
        {
            int value = 0;

            if (constant == this->m_constants.end ())
                value = (*defvalue).get <int> ();
            else if ((*constant).second->is <CShaderConstantFloat> () == true)
                value = *(*constant).second->as <CShaderConstantFloat> ()->getValue ();
            else if ((*constant).second->is <CShaderConstantInteger> () == true)
                value = *(*constant).second->as <CShaderConstantInteger> ()->getValue ();

            parameter = new Variables::CShaderVariableInteger (value);
        }
        else if (type == "sampler2D")
        {
            // samplers can have special requirements, check what sampler we're working with and create definitions
            // if needed
            auto combo = data.find ("combo");
            auto textureName = data.find ("default");

            if (combo != data.end ())
            {
                // add the new combo to the list
                this->m_combos->insert (std::make_pair <std::string, int> (*combo, 1));

                if (textureName != data.end ())
                {
                    // also ensure that the textureName is loaded and we know about it
                    ITexture* texture = this->m_container->readTexture ((*textureName).get <std::string> ());
                    // extract the texture number from the name
                    char value = name.at (std::string("g_Texture").length ());
                    // now convert it to integer
                    int index = value - '0';

                    this->m_textures.insert (
                            std::make_pair (index, texture)
                    );
                }
            }

            // samplers are not saved, we can ignore them for now
            return;
        }
        else
        {
            this->m_error = true;
            this->m_errorInfo = "Unknown parameter type: " + type + " for " + name;
            return;
        }

        if (material != data.end ())
        {
            parameter->setIdentifierName (*material);
            parameter->setName (name);

            this->m_parameters.push_back (parameter);
        }
    }

    Variables::CShaderVariable* Compiler::findParameter (const std::string& identifier)
    {
        auto cur = this->m_parameters.begin ();
        auto end = this->m_parameters.end ();

        for (; cur != end; cur ++)
        {
            if ((*cur)->getIdentifierName () == identifier)
            {
                return (*cur);
            }
        }

        return nullptr;
    }

    const std::vector <Variables::CShaderVariable*>& Compiler::getParameters () const
    {
        return this->m_parameters;
    }

    std::map <std::string, int>* Compiler::getCombos () const
    {
        return this->m_combos;
    }

    const std::map <int, ITexture*>& Compiler::getTextures () const
    {
        return this->m_textures;
    }

    std::vector<std::string> Compiler::sTypes =
    {
        "vec4", "uvec4", "ivec4", "dvec4", "bvec4",
        "vec3", "uvec3", "ivec3", "dvec3", "bvec3",
        "vec2", "uvec2", "ivec2", "dvec2", "bvec2",
        "float", "sampler2D", "mat4x3", "mat4", "mat3", "uint4",
        "void"
    };
}
