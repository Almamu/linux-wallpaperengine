#include "CShaderVariableVector3.h"

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector3::CShaderVariableVector3 (const irr::core::vector3df& defaultValue) :
    m_defaultValue (defaultValue),
    m_value (irr::core::vector3df ()),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
}

CShaderVariableVector3::CShaderVariableVector3 (const irr::core::vector3df& defaultValue, std::string name) :
    m_defaultValue (defaultValue),
    m_value (irr::core::vector3df ()),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
    this->setName (name);
}

void CShaderVariableVector3::setValue (const irr::core::vector3df& value)
{
    this->m_value = value;
    CShaderVariable::setValue (&this->m_value);
}

const int CShaderVariableVector3::getSize () const
{
    return 3;
}

const std::string CShaderVariableVector3::Type = "vec3";