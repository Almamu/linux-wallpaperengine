#include "CContainer.h"
#include "CAssetLoadException.h"
#include "CTexture.h"
#include "WallpaperEngine/Logging/CLog.h"

#include <cstring>
#include <filesystem>
#include <utility>

using namespace WallpaperEngine::Assets;

std::filesystem::path CContainer::resolveRealFile (const std::string& filename) const {
    throw CAssetLoadException (filename, "Cannot resolve physical file in this container");
}

const ITexture* CContainer::readTexture (const std::string& filename) const {
    // get the texture's filename (usually .tex)
    const std::string texture = "materials/" + filename + ".tex";

    const void* textureContents = this->readFile (texture, nullptr);

    const ITexture* result = new CTexture (textureContents);

#if !NDEBUG
    glObjectLabel (GL_TEXTURE, result->getTextureID (), -1, texture.c_str ());
#endif /* NDEBUG */

    return result;
}

std::string CContainer::readShader (const std::string& filename) const {
    std::filesystem::path shader = filename;
    auto it = shader.begin ();

    // detect workshop shaders and check if there's a
    if (*it++ == "workshop") {
        const std::filesystem::path workshopId = *it++;

        if (++it != shader.end ()) {
            const std::filesystem::path shaderfile = *it;

            try {
                shader = std::filesystem::path ("zcompat") / "scene" / "shaders" / workshopId / shaderfile;
                // replace the old path with the new one
                std::string contents = this->readFileAsString (shader);

                sLog.out ("Replaced ", filename, " with compat ", shader);

                return contents;
            } catch (CAssetLoadException&) {}
        }
    }

    return this->readFileAsString ("shaders/" + filename);
}

std::string CContainer::readVertexShader (const std::string& filename) const {
    return this->readShader (filename + ".vert");
}

std::string CContainer::readFragmentShader (const std::string& filename) const {
    return this->readShader (filename + ".frag");
}

std::string CContainer::readIncludeShader (const std::string& filename) const {
    return this->readFileAsString ("shaders/" + filename);
}

std::string CContainer::readFileAsString (const std::string& filename) const {
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
    delete [] buffer;

    return result;
}