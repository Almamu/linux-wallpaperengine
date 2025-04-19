#include "CShaderVariable.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariable::CShaderVariable (std::string type) :
    m_type (std::move (type)) {}

const std::string& CShaderVariable::getIdentifierName () const {
    return this->m_identifierName;
}

const std::string& CShaderVariable::getName () const {
    return this->m_name;
}

const std::string& CShaderVariable::getType () const {
    return this->m_type;
}

void CShaderVariable::setIdentifierName (std::string identifierName) {
    this->m_identifierName = std::move (identifierName);
}

void CShaderVariable::setName (const std::string& name) {
    this->m_name = name;
}