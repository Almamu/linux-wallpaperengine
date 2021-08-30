#include "CShaderVariableVector3.h"

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector3::CShaderVariableVector3 (const glm::vec3& defaultValue) :
    m_defaultValue (defaultValue),
    m_value (glm::vec3 ()),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
}

CShaderVariableVector3::CShaderVariableVector3 (const glm::vec3& defaultValue, std::string name) :
    m_defaultValue (defaultValue),
    m_value (glm::vec3 ()),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
    this->setName (name);
}

void CShaderVariableVector3::setValue (const glm::vec3& value)
{
    this->m_value = value;
    CShaderVariable::setValue (&this->m_value);
}

const int CShaderVariableVector3::getSize () const
{
    return 3;
}

const std::string CShaderVariableVector3::Type = "vec3";