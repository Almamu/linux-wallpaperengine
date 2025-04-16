#include "CShaderConstantVector4.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantVector4::CShaderConstantVector4 (glm::vec4 value) :
    CShaderConstant (Type),
    m_value (value) {}

const glm::vec4* CShaderConstantVector4::getValue () const {
    return &this->m_value;
}

std::string CShaderConstantVector4::toString () const {
    std::string result = "(";

    result.append (std::to_string (this->m_value.x));
    result.append (",");
    result.append (std::to_string (this->m_value.y));
    result.append (",");
    result.append (std::to_string (this->m_value.z));
    result.append (",");
    result.append (std::to_string (this->m_value.w));
    result.append (")");

    return result;
}

const std::string CShaderConstantVector4::Type = "vec4";