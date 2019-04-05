#include <irrlicht/irrlicht.h>
#include <iostream>
#include <fstream>
#include <string>

// system configuration
#include <wallpaperengine/config.h>

// filesystem
#include <wallpaperengine/fs/utils.h>

// video engine
#include <wallpaperengine/irrlicht.h>

// shader compiler
#include <wallpaperengine/shaders/compiler.h>

namespace wp
{
    namespace shaders
    {
        compiler::compiler (irr::io::path& file, Type type, bool recursive)
        {
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
                                    "#define atan2 atan\n"
                                    "#define ddx dFdx\n"
                                    "#define ddy(x) dFdy(-(x))\n"
                                    "#define GLSL 1\n\n";
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
            compiler loader (shader, this->m_type, true);

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
        #define BREAK_IF_ERROR if (this->m_error == true) { wp::irrlicht::device->getLogger ()->log ("ERROR PRE-COMPILING SHADER"); wp::irrlicht::device->getLogger ()->log (this->m_errorInfo.c_str ()); return ""; }
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
                        this->ignoreUpToNextLineFeed (it);
                        this->m_compiledContent.append (begin, it);
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

            wp::irrlicht::device->getLogger ()->log ("Compiled shader output for", this->m_file.c_str ());
            wp::irrlicht::device->getLogger ()->log (this->m_compiledContent.c_str ());

            return this->m_compiledContent;
        #undef BREAK_IF_ERROR
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
            "vec4", "vec3", "vec2", "float"
        };
    }
}
