#include "CShaderVariableVector4.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector4::CShaderVariableVector4 (const glm::vec3& defaultValue) :
    m_defaultValue (defaultValue),
    m_value (glm::vec3 ()),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
}

CShaderVariableVector4::CShaderVariableVector4 (const glm::vec3& defaultValue, std::string name) :
    m_defaultValue (defaultValue),
    m_value (glm::vec3 ()),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
    this->setName (std::move(name));
}

void CShaderVariableVector4::setValue (const glm::vec3& value)
{
    this->m_value = value;
    CShaderVariable::setValue (&this->m_value);
}

const int CShaderVariableVector4::getSize () const
{
    return 4;
}

const std::string CShaderVariableVector4::Type = "vec4";