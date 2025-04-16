#include "CShaderConstantFloat.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantFloat::CShaderConstantFloat (float value) :
    CShaderConstant (Type),
    m_value (value) {}

const float* CShaderConstantFloat::getValue () const {
    return &this->m_value;
}

std::string CShaderConstantFloat::toString () const {
    return std::to_string (this->m_value);
}

const std::string CShaderConstantFloat::Type = "float";