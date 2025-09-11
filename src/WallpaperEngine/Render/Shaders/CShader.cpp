#include <fstream>
#include <string>
#include <utility>

// shader compiler
#include <WallpaperEngine/Render/Shaders/CShader.h>
#include <regex>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

#include "CGLSLContext.h"
#include "WallpaperEngine/Assets/CAssetLoadException.h"
#include "WallpaperEngine/FileSystem/Container.h"

using namespace WallpaperEngine::Assets;

namespace WallpaperEngine::Render::Shaders {
// TODO: MOVE THIS INTO AN ASSET RESOLVER OR SOMETHING SIMILAR AS IT DOESN'T REALLY BELONG HERE BUT GETS THE CHANGESET OUT
std::string readShader (const std::filesystem::path& filename, const Container& container) {
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
                std::string contents = container.readString (shader);

                sLog.out ("Replaced ", filename, " with compat ", shader);

                return contents;
            } catch (CAssetLoadException&) {

            }
        }
    }

    return container.readString ("shaders" / filename);
}

std::string readVertex (const std::filesystem::path& filename, const Container& container) {
    std::filesystem::path shader = filename;
    shader.replace_extension (".vert");
    return readShader (shader, container);
}

std::string readFragment (const std::filesystem::path& filename, const Container& container) {
    std::filesystem::path shader = filename;
    shader.replace_extension (".frag");
    return readShader (shader, container);
}

CShader::CShader (
    const Container& container, std::string filename,
    const ComboMap& combos, const ComboMap& overrideCombos,
    const TextureMap& textures, const TextureMap& overrideTextures,
    const ShaderConstantMap& constants
) :
    m_vertex (
        CGLSLContext::UnitType_Vertex, filename, readVertex (filename, container),
        container, constants, textures, overrideTextures, combos, overrideCombos),
    m_fragment (
        CGLSLContext::UnitType_Fragment, filename, readFragment (filename, container),
        container, constants, textures, overrideTextures, combos, overrideCombos),
    m_file (std::move (filename)),
    m_combos (combos),
    m_overrideCombos (overrideCombos),
    m_passTextures (textures),
    m_overrideTextures (overrideTextures) {
    // link shaders between them
    this->m_vertex.linkToUnit (&this->m_fragment);
    this->m_fragment.linkToUnit (&this->m_vertex);
}


const std::string& CShader::vertex () {
    return this->m_vertex.compile ();
}

const std::string& CShader::fragment () {
    return this->m_fragment.compile ();
}

const CShaderUnit& CShader::getVertex () const {
    return this->m_vertex;
}

const CShaderUnit& CShader::getFragment () const {
    return this->m_fragment;
}

const std::map<std::string, int>& CShader::getCombos () const {
    return this->m_combos;
}

CShader::ParameterSearchResult CShader::findParameter (const std::string& name) {
    Variables::CShaderVariable* vertex = nullptr;
    Variables::CShaderVariable* fragment = nullptr;

    for (const auto& cur : this->m_vertex.getParameters ()) {
        if (cur->getIdentifierName () == name) {
            vertex = cur;
            break;
        }
    }

    for (const auto& cur : this->m_fragment.getParameters()) {
        if (cur->getIdentifierName () == name) {
            fragment = cur;
            break;
        }
    }

    return {
        .vertex = vertex,
        .fragment = fragment,
    };
}
} // namespace WallpaperEngine::Render::Shaders
