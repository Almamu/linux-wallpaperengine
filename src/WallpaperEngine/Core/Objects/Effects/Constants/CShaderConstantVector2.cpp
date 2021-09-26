#include "CShaderConstantVector2.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;


CShaderConstantVector2::CShaderConstantVector2 (glm::vec2 value) :
    CShaderConstant (Type),
    m_value (value)
{
}

glm::vec2* CShaderConstantVector2::getValue ()
{
    return &this->m_value;
}

const std::string CShaderConstantVector3::Type = "vec2";