#ifndef __BASIC_SHADER_LOADER_H__
#define __BASIC_SHADER_LOADER_H__

#include <irrlicht/irrlicht.h>
#include <iostream>
#include <vector>
#include <map>

/**
 * A basic shader loader that adds basic function definitions to every loaded shader
 */
class BasicShaderLoader
{
public:
    struct VariableReplacement
    {
        const char* original;
        const char* replacement;
    };

    struct TypeName
    {
        const char* name;
        int size;
    };

    enum Type
    {
        Type_Vertex = 0,
        Type_Pixel = 1,
    };

    static std::map<std::string, std::string> sVariableReplacement;
    static std::vector<std::string> sTypes;

    BasicShaderLoader (irr::io::path file, Type type, bool recursive = false);
    std::string* precompile ();

private:
    bool peekString (std::string str, std::string::const_iterator& it);
    bool expectSemicolon (std::string::const_iterator& it);
    void ignoreSpaces (std::string::const_iterator& it);
    void ignoreUpToNextLineFeed (std::string::const_iterator& it);
    void ignoreUpToBlockCommentEnd (std::string::const_iterator& it);
    std::string extractType (std::string::const_iterator& it);
    std::string extractName (std::string::const_iterator& it);
    std::string extractQuotedValue (std::string::const_iterator& it);
    std::string lookupShaderFile (std::string filename);
    std::string lookupReplaceSymbol (std::string symbol);

    bool isChar (std::string::const_iterator& it);
    bool isNumeric (std::string::const_iterator& it);

    std::string m_content;
    std::string m_compiledContent;
    bool m_error;
    std::string m_errorInfo;
    Type m_type;
};

#endif /* !__BASIC_SHADER_LOADER_H__ */