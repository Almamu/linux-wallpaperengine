#include "CShaderVariableInteger.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableInteger::CShaderVariableInteger(irr::s32 defaultValue) :
    m_defaultValue (defaultValue),
    m_value (0),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
}

CShaderVariableInteger::CShaderVariableInteger(irr::s32 defaultValue, std::string name) :
    m_defaultValue (defaultValue),
    m_value (0),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
    this->setName (std::move(name));
}

void CShaderVariableInteger::setValue (irr::s32 value)
{
    this->m_value = value;
    CShaderVariable::setValue (&this->m_value);
}

const int CShaderVariableInteger::getSize () const
{
    return 1;
}

const std::string CShaderVariableInteger::Type = "int";