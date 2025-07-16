#include "CContainer.h"
#include "CAssetLoadException.h"
#include "CTexture.h"
#include "WallpaperEngine/Logging/CLog.h"

#include <cstring>
#include <filesystem>
#include <utility>

using namespace WallpaperEngine::Assets;

std::filesystem::path CContainer::resolveRealFile (const std::filesystem::path& filename) const {
    throw CAssetLoadException (filename, "Cannot resolve physical file in this container");
}

std::shared_ptr <const ITexture> CContainer::readTexture (const std::filesystem::path& filename) const {
    // get the texture's filename (usually .tex)
    std::filesystem::path texture = "materials" / std::filesystem::path (filename.string ().append (".tex"));

    const auto textureContents = this->readFile (texture, nullptr);
    const auto result = std::make_shared<CTexture> (textureContents);

#if !NDEBUG
    glObjectLabel (GL_TEXTURE, result->getTextureID (0), -1, texture.c_str ());
#endif /* NDEBUG */

    return result;
}

std::string CContainer::readShader (const std::filesystem::path& filename) const {
    std::filesystem::path shader = filename;
    auto it = shader.begin ();

    // detect workshop shaders and check if there's a
    if (*it++ == "workshop") {
        const std::filesystem::path workshopId = *it++;

        if (++it != shader.end ()) {
            const std::filesystem::path& shaderfile = *it;

            try {
                shader = std::filesystem::path ("zcompat") / "scene" / "shaders" / workshopId / shaderfile;
                // replace the old path with the new one
                std::string contents = this->readFileAsString (shader);

                sLog.out ("Replaced ", filename, " with compat ", shader);

                return contents;
            } catch (CAssetLoadException&) {}
        }
    }

    return this->readFileAsString ("shaders" / filename);
}

std::string CContainer::readVertexShader (const std::filesystem::path& filename) const {
    std::filesystem::path shader = filename;
    shader.replace_extension (".vert");
    return this->readShader (shader);
}

std::string CContainer::readFragmentShader (const std::filesystem::path& filename) const {
    std::filesystem::path shader = filename;
    shader.replace_extension (".frag");
    return this->readShader (shader);
}

std::string CContainer::readIncludeShader (const std::filesystem::path& filename) const {
    return this->readFileAsString ("shaders" / filename);
}

std::string CContainer::readFileAsString (const std::filesystem::path& filename) const {
    uint32_t length = 0;

    return {
        reinterpret_cast<const char*> (this->readFile (filename, &length).get ()), length
    };
}