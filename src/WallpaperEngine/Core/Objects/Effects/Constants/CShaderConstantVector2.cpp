#include "CShaderConstantVector2.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantVector2::CShaderConstantVector2 (glm::vec2 value) :
    CShaderConstant (Type),
    m_value (value) {}

const glm::vec2* CShaderConstantVector2::getValue () const {
    return &this->m_value;
}

const std::string CShaderConstantVector2::Type = "vec2";