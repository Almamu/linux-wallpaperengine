#include "CShaderConstantInteger.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantInteger::CShaderConstantInteger (int32_t value) :
    CShaderConstant (Type),
    m_value (value) {}

const int32_t* CShaderConstantInteger::getValue () const {
    return &this->m_value;
}

std::string CShaderConstantInteger::toString () const {
    return std::to_string (this->m_value);
}

const std::string CShaderConstantInteger::Type = "int";