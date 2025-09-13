#include "AssetLocator.h"

#include "AssetLoadException.h"

using namespace WallpaperEngine::Assets;

AssetLocator::AssetLocator (ContainerUniquePtr filesystem) : m_filesystem (std::move (filesystem)) {}

std::string AssetLocator::shader (const std::filesystem::path& filename) const {
    try {
        std::filesystem::path shader = filename;

        // detect workshop shaders and check if there's a
        if (auto it = shader.begin (); *it++ == "workshop") {
            const std::filesystem::path workshopId = *it++;

            if (++it != shader.end ()) {
                const std::filesystem::path& shaderfile = *it;

                try {
                    shader = std::filesystem::path ("zcompat") / "scene" / "shaders" / workshopId / shaderfile;
                    // replace the old path with the new one
                    std::string contents = this->m_filesystem->readString (shader);

                    sLog.out ("Replaced ", filename, " with compat ", shader);

                    return contents;
                } catch (std::filesystem::filesystem_error&) {
                    // these exceptions can be ignored because the replacement file might not exist
                }
            }
        }

        return this->m_filesystem->readString ("shaders" / filename);
    } catch (std::filesystem::filesystem_error& base) {
        throw AssetLoadException (base);
    }
}

std::string AssetLocator::fragmentShader (const std::filesystem::path& filename) const {
    auto final = filename;

    final.replace_extension ("frag");

    return this->shader (final);
}

std::string AssetLocator::vertexShader (const std::filesystem::path& filename) const {
    auto final = filename;

    final.replace_extension ("vert");

    return this->shader (final);
}

std::string AssetLocator::includeShader (const std::filesystem::path& filename) const {
    auto final = filename;

    final.replace_extension ("h");

    return this->shader (final);
}

std::string AssetLocator::readString (const std::filesystem::path& filename) const {
    try {
        return this->m_filesystem->readString (filename);
    } catch (std::filesystem::filesystem_error& base) {
        throw AssetLoadException (base);
    }
}


ReadStreamSharedPtr AssetLocator::texture (const std::filesystem::path& filename) const {
    auto final = filename;

    final.replace_extension ("tex");

    try {
        return this->m_filesystem->read (final);
    } catch (std::filesystem::filesystem_error& base) {
        throw AssetLoadException (base);
    }
}

ReadStreamSharedPtr AssetLocator::read (const std::filesystem::path& path) const {
    try {
        return this->m_filesystem->read (path);
    } catch (std::filesystem::filesystem_error& base) {
        throw AssetLoadException (base);
    }
}

std::filesystem::path AssetLocator::physicalPath (const std::filesystem::path& path) const {
    try {
        return this->m_filesystem->physicalPath (path);
    } catch (std::filesystem::filesystem_error& base) {
        throw AssetLoadException (base);
    }
}