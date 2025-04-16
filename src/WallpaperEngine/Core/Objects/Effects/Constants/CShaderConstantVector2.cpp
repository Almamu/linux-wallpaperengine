#include "CShaderConstantVector2.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantVector2::CShaderConstantVector2 (glm::vec2 value) :
    CShaderConstant (Type),
    m_value (value) {}

const glm::vec2* CShaderConstantVector2::getValue () const {
    return &this->m_value;
}

std::string CShaderConstantVector2::toString () const {
    std::string result = "(";

    result.append (std::to_string (this->m_value.x));
    result.append (",");
    result.append (std::to_string (this->m_value.y));
    result.append (")");

    return result;
}

const std::string CShaderConstantVector2::Type = "vec2";