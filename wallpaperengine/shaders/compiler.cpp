#include <irrlicht/irrlicht.h>
#include <iostream>
#include <fstream>
#include <string>

// filesystem
#include <wallpaperengine/fs/utils.h>

// video engine
#include <wallpaperengine/irrlicht.h>

// shader compiler
#include <wallpaperengine/shaders/compiler.h>
#include <wallpaperengine/core.h>

namespace wp
{
    namespace shaders
    {
        compiler::compiler (irr::io::path& file, Type type, std::map<std::string, int>* combos, bool recursive)
        {
            this->m_recursive = recursive;
            this->m_combos = combos;

            // begin with an space so it gets ignored properly on parse
            if (recursive == false)
            {
                // compatibility layer for OpenGL shaders
                this->m_content =   "#version 120\n"
                                    "#define highp\n"
                                    "#define mediump\n"
                                    "#define lowp\n"
                                    "#define mul(x, y) (y * x)\n"
                                    "#define frac fract\n"
                                    "#define CAST2(x) (vec2(x))\n"
                                    "#define CAST3(x) (vec3(x))\n"
                                    "#define CAST4(x) (vec4(x))\n"
                                    "#define CAST3X3(x) (mat3(x))\n"
                                    "#define saturate(x) (clamp(x, 0.0, 1.0))\n"
                                    "#define texSample2D texture2D\n"
                                    "#define texSample2DLod texture2DLod\n"
                                    "#define texture2DLod texture2D\n"
                                    "#define atan2 atan\n"
                                    "#define ddx dFdx\n"
                                    "#define ddy(x) dFdy(-(x))\n"
                                    "#define GLSL 1\n\n";

                std::map<std::string, int>::const_iterator cur = this->m_combos->begin ();
                std::map<std::string, int>::const_iterator end = this->m_combos->end ();

                for (; cur != end; cur ++)
                {
                    this->m_content += "#define " + (*cur).first + " " + std::to_string ((*cur).second) + "\n";
                }
            }
            else
            {
                this->m_content = "";
            }

            this->m_content.append (wp::fs::utils::loadFullFile (file));

            // append file content
            this->m_type = type;

            this->m_file = file;
        }

        bool compiler::peekString(std::string str, std::string::const_iterator& it)
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

        bool compiler::expectSemicolon (std::string::const_iterator& it)
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

        void compiler::ignoreSpaces(std::string::const_iterator &it)
        {
            while (it != this->m_content.end() && (*it == ' ' || *it == '\t')) it ++;
        }

        void compiler::ignoreUpToNextLineFeed (std::string::const_iterator& it)
        {
            while (it != this->m_content.end() && *it != '\n') it ++;
        }

        void compiler::ignoreUpToBlockCommentEnd (std::string::const_iterator& it)
        {
            while (it != this->m_content.end() && this->peekString ("*/", it) == false) it ++;
        }

        std::string compiler::extractType (std::string::const_iterator& it)
        {
            std::vector<std::string>::const_iterator cur = sTypes.begin ();
            std::vector<std::string>::const_iterator end = sTypes.end ();

            while (cur != end)
            {
                if (this->peekString (*cur, it) == true)
                {
                    return *cur;
                }

                cur ++;
            }

            this->m_error = true;
            this->m_errorInfo = "Expected type";
            return "";
        }

        std::string compiler::extractName (std::string::const_iterator& it)
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

        bool compiler::isChar (std::string::const_iterator& it)
        {
            return ((*it) >= 'A' && (*it) <= 'Z') || ((*it) >= 'a' && (*it) <= 'z');
        }

        bool compiler::isNumeric (std::string::const_iterator& it)
        {
            return (*it) >= '0' && (*it) <= '9';
        }

        std::string compiler::extractQuotedValue(std::string::const_iterator& it)
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

        std::string compiler::lookupShaderFile (std::string filename)
        {
            // get file information
            irr::io::path shader = ("shaders/" + filename).c_str ();

            if (shader == "")
            {
                this->m_error = true;
                this->m_errorInfo = "Cannot find file " + filename + " to include";
                return "";
            }

            // now compile the new shader
            // do not include the default header (as it's already included in the parent)
            compiler loader (shader, this->m_type, this->m_combos, true);

            return loader.precompile ();
        }

        std::string compiler::lookupReplaceSymbol (std::string symbol)
        {
            std::map<std::string, std::string>::const_iterator cur = sVariableReplacement.begin ();
            std::map<std::string, std::string>::const_iterator end = sVariableReplacement.end ();

            while (cur != end)
            {
                if (cur->first == symbol)
                {
                    return cur->second;
                }

                cur ++;
            }

            // if there is no replacement, return the original
            return symbol;
        }

        std::string compiler::precompile()
        {
        #define BREAK_IF_ERROR if (this->m_error == true) { wp::irrlicht::device->getLogger ()->log ("ERROR PRE-COMPILING SHADER", irr::ELL_ERROR); wp::irrlicht::device->getLogger ()->log (this->m_errorInfo.c_str (), irr::ELL_ERROR); return ""; }
            // parse the shader and find #includes and such things and translate them to the correct name
            // also remove any #version definition to prevent errors
            std::string::const_iterator it = this->m_content.begin ();

            // reset error indicator
            this->m_error = false;
            this->m_errorInfo = "";

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
                    if (this->peekString ("#include", it) == true)
                    {
                        std::string filename = "";

                        // ignore whitespaces
                        this->ignoreSpaces (it); BREAK_IF_ERROR
                        // extract value between quotes
                        filename = this->extractQuotedValue (it); BREAK_IF_ERROR

                        // try to find the file first
                        this->m_compiledContent += "// begin of included from file " + filename + "\r\n";
                        this->m_compiledContent += this->lookupShaderFile (filename);
                        this->m_compiledContent += "\r\n// end of included from file " + filename + "\r\n";
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
                    if (this->peekString ("uniform", it) == true)
                    {
                        this->ignoreSpaces (it);
                        std::string type = this->extractType (it); BREAK_IF_ERROR
                        this->ignoreSpaces (it);
                        std::string name = this->extractName (it); BREAK_IF_ERROR
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
                            this->m_compiledContent += "uniform " + type + " " + name + "; // " + configuration;
                        }
                        else
                        {
                            this->m_compiledContent += "uniform " + type + " " + name + ";";
                        }
                    }
                }
                else if (*it == 'a')
                {
                    // find attribute definitions
                    if (this->peekString ("attribute", it) == true)
                    {
                        this->ignoreSpaces (it);
                        std::string type = this->extractType (it); BREAK_IF_ERROR
                        this->ignoreSpaces (it);
                        std::string name = this->extractName (it); BREAK_IF_ERROR
                        this->ignoreSpaces (it);
                        this->expectSemicolon (it); BREAK_IF_ERROR

                        this->m_compiledContent += "// attribute";
                        this->m_compiledContent += " " + type + " ";
                        this->m_compiledContent += name;
                        this->m_compiledContent += "; /* replaced by " + this->lookupReplaceSymbol (name) + " */";
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
                                this->m_compiledContent += this->lookupReplaceSymbol (name);
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

                            this->parseComboConfiguration (configuration); BREAK_IF_ERROR;
                        }
                        else if (this->peekString ("[COMBO_OFF]", it) == true)
                        {
                            // parse combo json data to define the proper variables
                            this->ignoreSpaces (it);
                            begin = it;
                            this->ignoreUpToNextLineFeed (it);

                            std::string configuration; configuration.append (begin, it);

                            this->m_compiledContent += "// [COMBO_OFF] " + configuration;

                            this->parseComboConfiguration (configuration); BREAK_IF_ERROR;
                        }
                        else
                        {
                            this->ignoreUpToNextLineFeed (it);
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
                        this->m_compiledContent += type;
                    }
                    else
                    {
                        this->m_error = false;
                        std::string name = this->extractName (it);

                        if (this->m_error == false)
                        {
                            // check if the name is a translated one or not
                            this->m_compiledContent += this->lookupReplaceSymbol (name);
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

            if (this->m_recursive == false)
            {
                //wp::irrlicht::device->getLogger ()->log ("Compiled shader output for", this->m_file.c_str ());
                //wp::irrlicht::device->getLogger ()->log (this->m_compiledContent.c_str ());
            }

            return this->m_compiledContent;
        #undef BREAK_IF_ERROR
        }

        void compiler::parseComboConfiguration (const std::string& content)
        {
            json data = json::parse (content);
            json::const_iterator combo = data.find ("combo");
            json::const_iterator defvalue = data.find ("default");

            // add line feed just in case
            this->m_compiledContent += "\n";

            if (combo == data.end () || defvalue == data.end ())
            {
                wp::irrlicht::device->getLogger ()->log ("Cannot parse combo information", irr::ELL_ERROR);
                return;
            }

            // check the combos
            std::map<std::string, int>::const_iterator entry = this->m_combos->find ((*combo).get <std::string> ());

            // if the combo was not found in the predefined values this means that the default value in the JSON data can be used
            // so only define the ones that are not already defined
            if (entry == this->m_combos->end ())
            {
                // if no combo is defined just load the default settings
                if ((*defvalue).is_number_float ())
                {
                    this->m_compiledContent += "#define " + (*combo).get <std::string> () + " " + std::to_string ((*defvalue).get <irr::f32> ()) + "\n";
                }
                else if ((*defvalue).is_number_integer ())
                {
                    this->m_compiledContent += "#define " + (*combo).get <std::string> () + " " + std::to_string ((*defvalue).get <irr::s32> ()) + "\n";
                }
                else if ((*defvalue).is_string ())
                {
                    this->m_compiledContent += "#define " + (*combo).get <std::string> () + " " + (*defvalue).get <std::string> () + "\n";
                }
                else
                {
                    wp::irrlicht::device->getLogger ()->log ("Cannot parse combo information, unknown type", irr::ELL_ERROR);
                }
            }
        }

        void compiler::parseParameterConfiguration (const std::string& type, const std::string& name, const std::string& content)
        {
            json data = json::parse (content);
            json::const_iterator material = data.find ("material");
            json::const_iterator defvalue = data.find ("default");
            json::const_iterator range = data.find ("range");

            // this is not a real parameter
            if (material == data.end () || defvalue == data.end ())
            {
                if (type != "sampler2D")
                    wp::irrlicht::device->getLogger ()->log ("Cannot parse parameter info for ", name.c_str (), irr::ELL_ERROR);

                return;
            }

            ShaderParameter* param = new ShaderParameter;

            param->identifierName = (*material).get <std::string> ();
            param->variableName = name;
            param->type = type;

            if (type == "vec4" || type == "vec3")
            {
                if ((*defvalue).is_string () == false)
                {
                    irr::core::vector3df* vector = new irr::core::vector3df;

                    vector->X = 0.0f;
                    vector->Y = 0.0f;
                    vector->Z = 0.0f;

                    param->defaultValue = vector;
                }
                else
                {
                    irr::core::vector3df tmp = wp::core::ato3vf ((*defvalue).get <std::string> ().c_str ());
                    irr::core::vector3df* vector = new irr::core::vector3df;

                    vector->X = tmp.X;
                    vector->Y = tmp.Y;
                    vector->Z = tmp.Z;

                    param->defaultValue = vector;
                }
            }
            else if (type == "vec2")
            {
                if ((*defvalue).is_string () == false)
                {
                    irr::core::vector2df* vector = new irr::core::vector2df;

                    vector->X = 0.0f;
                    vector->Y = 0.0f;

                    param->defaultValue = vector;
                }
                else
                {
                    irr::core::vector2df* vector = new irr::core::vector2df;
                    irr::core::vector2df tmp = wp::core::ato2vf ((*defvalue).get <std::string> ().c_str ());

                    vector->X = tmp.X;
                    vector->Y = tmp.Y;

                    param->defaultValue = vector;
                }
            }
            else if (type == "float")
            {
                if ((*defvalue).is_number () == false)
                {
                    irr::f32* val = new irr::f32;

                    *val = 0.0f;

                    param->defaultValue = val;

                }
                else
                {
                    irr::f32* val = new irr::f32;

                    *val = (*defvalue).get <irr::f32> ();

                    param->defaultValue = val;
                }
            }
            else if (type == "sampler2D")
            {
                // samplers are not saved, we can ignore them for now
                delete param;
                return;
            }
            else
            {
                this->m_error = true;
                this->m_errorInfo = "Unknown parameter type: " + type + " for " + param->identifierName + " (" + param->variableName + ")";
                return;
            }

            this->m_parameters.push_back (param);
        }

        compiler::ShaderParameter* compiler::findParameter (std::string identifier)
        {
            std::vector<ShaderParameter*>::const_iterator cur = this->m_parameters.begin ();
            std::vector<ShaderParameter*>::const_iterator end = this->m_parameters.end ();

            for (; cur != end; cur ++)
            {
                if ((*cur)->identifierName == identifier)
                {
                    return (*cur);
                }
            }

            return nullptr;
        }

        std::vector <compiler::ShaderParameter*>& compiler::getParameters ()
        {
            return this->m_parameters;
        }

        std::map<std::string, std::string>  compiler::sVariableReplacement =
        {
            // attribute vec3 a_position
            {"a_Position", "gl_Vertex.xyz"},
            // attribute vec2 a_TexCoord
            {"a_TexCoord", "gl_MultiTexCoord0.xy"},
            // attribute vec3 a_Normal
            {"a_Normal", "gl_Normal.xyz"}
        };

        std::vector<std::string> compiler::sTypes =
        {
            "vec4", "vec3", "vec2", "float", "sampler2D", "mat4"
        };
    }
}
