#include "CShaderVariable.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariable::CShaderVariable (void* defaultValue, void* value, std::string type) :
    m_defaultValue (defaultValue),
    m_value (value),
    m_type (std::move(type))
{

}

const void* CShaderVariable::getValue () const
{
    if (this->m_value)
        return this->m_value;

    return this->m_defaultValue;
}

void CShaderVariable::setValue (void* value)
{
    this->m_value = value;
}

const std::string& CShaderVariable::getIdentifierName () const
{
    return this->m_identifierName;
}

const std::string& CShaderVariable::getName () const
{
    return this->m_name;
}

const std::string& CShaderVariable::getType () const
{
    return this->m_type;
}

void CShaderVariable::setIdentifierName (std::string identifierName)
{
    this->m_identifierName = std::move(identifierName);
}

void CShaderVariable::setName (std::string name)
{
    this->m_name = std::move(name);
}