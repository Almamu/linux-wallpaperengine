#include "CShaderConstantFloat.h"

using namespace WallpaperEngine::Core::Objects::Effects;


CShaderConstantFloat::CShaderConstantFloat (irr::f32 value) :
    CShaderConstant (Type),
    m_value (value)
{
}

irr::f32* CShaderConstantFloat::getValue ()
{
    return &this->m_value;
}

const std::string CShaderConstantFloat::Type = "float";