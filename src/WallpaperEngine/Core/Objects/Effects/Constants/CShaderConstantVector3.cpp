#include "CShaderConstantVector3.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantVector3::CShaderConstantVector3 (glm::vec3 value) :
    CShaderConstant (Type),
    m_value (value) {}

const glm::vec3* CShaderConstantVector3::getValue () const {
    return &this->m_value;
}

const std::string CShaderConstantVector3::Type = "vec3";