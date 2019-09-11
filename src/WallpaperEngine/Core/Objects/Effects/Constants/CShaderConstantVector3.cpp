#include "CShaderConstantVector3.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;


CShaderConstantVector3::CShaderConstantVector3 (irr::core::vector3df value) :
    CShaderConstant (Type),
    m_value (value)
{
}

irr::core::vector3df* CShaderConstantVector3::getValue ()
{
    return &this->m_value;
}

const std::string CShaderConstantVector3::Type = "vector3";