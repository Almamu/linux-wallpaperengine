#include "CShaderVariableInteger.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableInteger::CShaderVariableInteger (int32_t defaultValue) :
    CShaderVariable (&this->m_defaultValue, nullptr, Type),
    m_defaultValue (defaultValue),
    m_value (0) {}

CShaderVariableInteger::CShaderVariableInteger (int32_t defaultValue, const std::string& name) :
    CShaderVariable (&this->m_defaultValue, nullptr, Type),
    m_defaultValue (defaultValue),
    m_value (0) {
    this->setName (name);
}

void CShaderVariableInteger::setValue (int32_t value) {
    this->m_value = value;
    CShaderVariable::setValue (&this->m_value);
}

const int CShaderVariableInteger::getSize () const {
    return 1;
}

const std::string CShaderVariableInteger::Type = "int";