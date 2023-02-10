#include "CContainer.h"
#include "CTexture.h"
#include "WallpaperEngine/Logging/CLog.h"
#include "CAssetLoadException.h"

#include <cstring>
#include <utility>
#include <filesystem>

using namespace WallpaperEngine::Assets;

const ITexture* CContainer::readTexture (std::string filename) const
{
    // get the texture's filename (usually .tex)
    filename = "materials/" + filename + ".tex";

    const void* textureContents = this->readFile (filename, nullptr);

    ITexture* result = new CTexture (textureContents);

#if DEBUG
        glObjectLabel (GL_TEXTURE, result->getTextureID (), -1, filename.c_str ());
#endif /* DEBUG */

    return result;
}
std::string CContainer::readShader (const std::string& filename) const
{
	std::filesystem::path shader = filename;
	auto it = shader.begin ();

	// detect workshop shaders and check if there's a
	if (*it++ == "workshop")
	{
		std::filesystem::path workshopId = *it++;
		std::filesystem::path shaderfile = *++it;

		try
		{
			shader = std::filesystem::path ("zcompat") / "scene" / "shaders" / workshopId / shaderfile;
			// replace the old path with the new one
			std::string contents = this->readFileAsString (shader);

			sLog.out ("Replaced ", filename, " with compat ", shader);

			return contents;
		}
		catch (CAssetLoadException&)
		{

		}
	}

	return this->readFileAsString ("shaders/" + filename);
}

std::string CContainer::readVertexShader (const std::string& filename) const
{
    return this->readShader (filename + ".vert");
}

std::string CContainer::readFragmentShader (const std::string& filename) const
{
    return this->readShader (filename + ".frag");
}

std::string CContainer::readIncludeShader (const std::string& filename) const
{
    return this->readFileAsString ("shaders/" + filename);
}

std::string CContainer::readFileAsString (std::string filename) const
{
    uint32_t length = 0;

    // read file contents and allocate a buffer for a string
    const void* contents = this->readFile (std::move (filename), &length);
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