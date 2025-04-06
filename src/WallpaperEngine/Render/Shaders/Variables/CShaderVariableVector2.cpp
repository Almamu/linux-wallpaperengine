#include "CShaderVariableVector2.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector2::CShaderVariableVector2 (const glm::vec2& defaultValue) :
    CShaderVariable (&this->m_defaultValue, nullptr, Type),
    m_defaultValue (defaultValue),
    m_value (glm::vec2 ()) {}

CShaderVariableVector2::CShaderVariableVector2 (const glm::vec2& defaultValue, const std::string& name) :
    CShaderVariable (&this->m_defaultValue, nullptr, Type),
    m_defaultValue (defaultValue),
    m_value (glm::vec2 ()) {
    this->setName (name);
}

void CShaderVariableVector2::setValue (const glm::vec2& value) {
    this->m_value = value;

    CShaderVariable::setValue (&this->m_value);
}

const int CShaderVariableVector2::getSize () const {
    return 2;
}

const std::string CShaderVariableVector2::Type = "vec2";