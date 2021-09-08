#include "CShaderConstantInteger.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;


CShaderConstantInteger::CShaderConstantInteger (int32_t value) :
    CShaderConstant (Type),
    m_value (value)
{
}

int32_t* CShaderConstantInteger::getValue ()
{
    return &this->m_value;
}

const std::string CShaderConstantInteger::Type = "int";