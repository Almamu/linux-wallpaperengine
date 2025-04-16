#include "CShaderConstantVector3.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantVector3::CShaderConstantVector3 (glm::vec3 value) :
    CShaderConstant (Type),
    m_value (value) {}

const glm::vec3* CShaderConstantVector3::getValue () const {
    return &this->m_value;
}

std::string CShaderConstantVector3::toString () const {
    std::string result = "(";

    result.append (std::to_string (this->m_value.x));
    result.append (",");
    result.append (std::to_string (this->m_value.y));
    result.append (",");
    result.append (std::to_string (this->m_value.z));
    result.append (")");

    return result;
}

const std::string CShaderConstantVector3::Type = "vec3";