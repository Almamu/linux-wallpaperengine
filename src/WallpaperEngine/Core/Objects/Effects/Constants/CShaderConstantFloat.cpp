#include "CShaderConstantFloat.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;


CShaderConstantFloat::CShaderConstantFloat (float value) :
    CShaderConstant (Type),
    m_value (value)
{
}

float* CShaderConstantFloat::getValue ()
{
    return &this->m_value;
}

const std::string CShaderConstantFloat::Type = "float";