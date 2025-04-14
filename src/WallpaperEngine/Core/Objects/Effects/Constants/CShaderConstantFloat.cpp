#include "CShaderConstantFloat.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantFloat::CShaderConstantFloat (float value) :
    CShaderConstant (Type),
    m_value (value) {}

const float* CShaderConstantFloat::getValue () const {
    return &this->m_value;
}

const std::string CShaderConstantFloat::Type = "float";