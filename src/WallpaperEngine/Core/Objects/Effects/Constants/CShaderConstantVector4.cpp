#include "CShaderConstantVector4.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantVector4::CShaderConstantVector4 (glm::vec4 value) :
    CShaderConstant (Type),
    m_value (value) {}

const glm::vec4* CShaderConstantVector4::getValue () const {
    return &this->m_value;
}

const std::string CShaderConstantVector4::Type = "vec4";