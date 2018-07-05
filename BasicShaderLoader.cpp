#include <irrlicht.h>
#include <iostream>
#include <fstream>
#include <string>
#include "BasicShaderLoader.h"
#include "nier_test.h"
#include "common.h"

BasicShaderLoader::BasicShaderLoader (irr::io::path file, Type type)
{
    this->m_content =	"#version 120\n"
                        "#define texSample2D texture2D\n"
                        "#define frac fract\n";

    std::ifstream _in (file.c_str ());
    this->m_content.append (std::istreambuf_iterator<char> (_in), std::istreambuf_iterator<char> ());
    this->m_type = type;
}

bool BasicShaderLoader::peekString(std::string str, std::string::const_iterator& it)
{
    std::string::const_iterator check = str.begin();
    std::string::const_iterator cur = it + 1;

    while (cur != this->m_content.end () && check != str.end ())
    {
        if (*cur != *check) return false;

        cur ++; check ++;
    }

    if (check != str.end ())
    {
        return false;
    }

    it = cur;

    return true;
}

void BasicShaderLoader::ignoreSpaces(std::string::const_iterator &it)
{
    while (it != this->m_content.end() && (*it == ' ' || *it == '\t')) it ++;
}

std::string BasicShaderLoader::extractQuotedValue(std::string::const_iterator& it)
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

    it = cur;
    return filename;
}

std::string BasicShaderLoader::lookupShaderFile (std::string filename)
{
    irr::io::path shaderTest = _example_base_folder; shaderTest += "/"; shaderTest += filename.c_str ();
    irr::io::IFileSystem *fs = device->getFileSystem ();

    // by default try to load from current folder
    // we might want, in the future, to load things from different folders
    // to provide standard functionality
    if (fs->existFile (shaderTest) == false)
    {
        this->m_error = true;
        this->m_errorInfo = "Cannot find file " + filename + " to include";
        return "";
    }

    // open the file, read content and close it
    irr::io::IReadFile* readFile = fs->createAndOpenFile (shaderTest);
    char* buffer = (char*) malloc (readFile->getSize () + 1);
    memset (buffer, 0, readFile->getSize() + 1);

    readFile->read (buffer, readFile->getSize ());
    readFile->drop ();

    std::string output = buffer;

    free (buffer);

    return output;
}

std::string* BasicShaderLoader::precompile ()
{
    // parse the shader and find #includes and such things and translate them to the correct name
    // also remove any #version definition to prevent errors
    std::string::const_iterator it = this->m_content.begin ();

    // search preprocessor macros and parse them
    while (it != this->m_content.end () && this->m_error == false)
    {
        // TODO: on precompilation steps search for attributes and replace them
        // TODO: with the correct opengl shader variable
        if (*it == '#')
        {
            if (this->peekString ("include", it) == true)
            {
                std::string filename = "";

                // ignore whitespaces
                this->ignoreSpaces(it);
                // extract value between quotes
                filename = this->extractQuotedValue(it);

                // try to find the file first
                this->m_compiledContent += "// begin of included from file " + filename + "\r\n";
                this->m_compiledContent += this->lookupShaderFile (filename);
                this->m_compiledContent += "\r\n// end of included from file " + filename + "\r\n";
            }
            else
            {
                this->m_compiledContent += '#';
            }
        }
        else
        {
            this->m_compiledContent += *it;
        }

        it ++;
    }

    return &this->m_compiledContent;
}