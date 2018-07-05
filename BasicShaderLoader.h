#ifndef __BASIC_SHADER_LOADER_H__
#define __BASIC_SHADER_LOADER_H__

#include <iostream>

/**
 * A basic shader loader that adds basic function definitions to every loaded shader
 */
class BasicShaderLoader
{
public:
    enum Type
    {
        Type_Vertex = 0,
        Type_Pixel = 1,
    };

    BasicShaderLoader (irr::io::path file, Type type);
    std::string* precompile ();

private:
    bool peekString (std::string str, std::string::const_iterator& it);
    void ignoreSpaces (std::string::const_iterator& it);
    std::string extractQuotedValue (std::string::const_iterator& it);
    std::string lookupShaderFile (std::string filename);

    std::string m_content;
    std::string m_compiledContent;
    bool m_error;
    std::string m_errorInfo;
    Type m_type;
};

#endif /* !__BASIC_SHADER_LOADER_H__ */