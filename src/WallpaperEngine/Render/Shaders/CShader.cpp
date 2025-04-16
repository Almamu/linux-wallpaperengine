#include <fstream>
#include <string>
#include <utility>

// shader compiler
#include <WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantFloat.h>
#include <WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantInteger.h>
#include <WallpaperEngine/Core/Objects/Effects/Constants/CShaderConstantVector4.h>
#include <WallpaperEngine/Render/Shaders/CShader.h>
#include <regex>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

#include "CGLSLContext.h"

using namespace WallpaperEngine::Core;
using namespace WallpaperEngine::Assets;

namespace WallpaperEngine::Render::Shaders {
CShader::CShader (
    const CContainer* container, std::string filename, std::map<std::string, int> combos,
    const std::map<int, std::string>& textures, const std::map<std::string, const CShaderConstant*>& constants
) :
    m_file (std::move (filename)),
    m_combos (std::move(combos)),
    m_passTextures (textures),
    m_vertex (
        CGLSLContext::UnitType_Vertex, filename, container->readVertexShader (filename),
        container, constants, textures, combos),
    m_fragment (
        CGLSLContext::UnitType_Fragment, filename, container->readFragmentShader (filename),
        container, constants, textures, combos) {
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
