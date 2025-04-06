#include "CShaderVariableVector4.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector4::CShaderVariableVector4 (const glm::vec4& defaultValue) :
    CShaderVariable (&this->m_defaultValue, nullptr, Type),
    m_defaultValue (defaultValue),
    m_value (glm::vec4 ()) {}

CShaderVariableVector4::CShaderVariableVector4 (const glm::vec4& defaultValue, const std::string& name) :
    CShaderVariable (&this->m_defaultValue, nullptr, Type),
    m_defaultValue (defaultValue),
    m_value (glm::vec4 ()) {
    this->setName (name);
}

void CShaderVariableVector4::setValue (const glm::vec4& value) {
    this->m_value = value;
    CShaderVariable::setValue (&this->m_value);
}

const int CShaderVariableVector4::getSize () const {
    return 4;
}

const std::string CShaderVariableVector4::Type = "vec4";