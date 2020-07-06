#include "CShaderVariableVector4.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector4::CShaderVariableVector4 (const irr::core::vector3df& defaultValue) :
    m_defaultValue (defaultValue),
    m_value (irr::core::vector3df ()),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
}

CShaderVariableVector4::CShaderVariableVector4 (const irr::core::vector3df& defaultValue, std::string name) :
    m_defaultValue (defaultValue),
    m_value (irr::core::vector3df ()),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
    this->setName (std::move(name));
}

void CShaderVariableVector4::setValue (const irr::core::vector3df& value)
{
    this->m_value = value;
    CShaderVariable::setValue (&this->m_value);
}

const int CShaderVariableVector4::getSize () const
{
    return 4;
}

const std::string CShaderVariableVector4::Type = "vec4";