#include "ShaderVariable.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

const std::string& ShaderVariable::getIdentifierName () const {
    return this->m_identifierName;
}

const std::string& ShaderVariable::getName () const {
    return this->m_name;
}


void ShaderVariable::setIdentifierName (std::string identifierName) {
    this->m_identifierName = std::move (identifierName);
}

void ShaderVariable::setName (const std::string& name) {
    this->m_name = name;
}