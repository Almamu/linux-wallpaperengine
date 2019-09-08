#include "CShaderConstantString.h"

#include <utility>

using namespace WallpaperEngine::Core::Objects::Effects;


CShaderConstantString::CShaderConstantString (std::string value) :
    CShaderConstant (Type),
    m_value (std::move(value))
{
}

std::string* CShaderConstantString::getValue ()
{
    return &this->m_value;
}

const std::string CShaderConstantString::Type = "string";