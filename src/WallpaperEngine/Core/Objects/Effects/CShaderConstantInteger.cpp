#include "CShaderConstantInteger.h"

using namespace WallpaperEngine::Core::Objects::Effects;


CShaderConstantInteger::CShaderConstantInteger (irr::s32 value) :
    CShaderConstant (Type),
    m_value (value)
{
}

irr::u32* CShaderConstantInteger::getValue ()
{
    return &this->m_value;
}

const std::string CShaderConstantInteger::Type = "integer";