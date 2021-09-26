#include "CContainer.h"
#include "CTexture.h"

#include <cstring>
#include <utility>

using namespace WallpaperEngine::Assets;

ITexture* CContainer::readTexture (std::string filename)
{
    // get the texture's filename (usually .tex)
    filename = "materials/" + filename + ".tex";

    void* textureContents = this->readFile (filename, nullptr);

    return new CTexture (textureContents);
}

std::string CContainer::readVertexShader (const std::string& filename)
{
    return this->readFileAsString ("shaders/" + filename + ".vert");
}

std::string CContainer::readFragmentShader (const std::string& filename)
{
    return this->readFileAsString ("shaders/" + filename + ".frag");
}

std::string CContainer::readIncludeShader (const std::string& filename)
{
    return this->readFileAsString ("shaders/" + filename);
}

std::string CContainer::readFileAsString (std::string filename)
{
    uint32_t length = 0;

    // read file contents and allocate a buffer for a string
    void* contents = this->readFile (std::move (filename), &length);
    char* buffer = new char [length + 1];

    // ensure there's a 0 at the end
    memset (buffer, 0, length + 1);
    // copy over the data
    memcpy (buffer, contents, length);
    // now build the std::string to use
    std::string result = buffer;

    // free the intermediate buffer used to generate the std::string
    delete[] buffer;

    return result;
}