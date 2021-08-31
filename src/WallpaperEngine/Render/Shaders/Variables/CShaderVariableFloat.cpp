#include "CShaderVariableFloat.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableFloat::CShaderVariableFloat(float defaultValue) :
    m_defaultValue (defaultValue),
    m_value (0),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
}

CShaderVariableFloat::CShaderVariableFloat(float defaultValue, std::string name) :
    m_defaultValue (defaultValue),
    m_value (0),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
    this->setName (std::move(name));
}

void CShaderVariableFloat::setValue (float value)
{
    this->m_value = value;

    CShaderVariable::setValue (&this->m_value);
}

const int CShaderVariableFloat::getSize () const
{
    return 1;
}

const std::string CShaderVariableFloat::Type = "float";