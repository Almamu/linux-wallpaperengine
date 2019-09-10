#include "CShaderParameter.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Parameters;

CShaderParameter::CShaderParameter (void* defaultValue, void* value, std::string type) :
    m_defaultValue (defaultValue),
    m_value (value),
    m_type (std::move(type))
{

}

const void* CShaderParameter::getValue () const
{
    if (this->m_value)
        return this->m_value;

    return this->m_defaultValue;
}

void CShaderParameter::setValue (void* value)
{
    this->m_value = value;
}

const std::string& CShaderParameter::getIdentifierName () const
{
    return this->m_identifierName;
}

const std::string& CShaderParameter::getName () const
{
    return this->m_name;
}

void CShaderParameter::setIdentifierName (std::string identifierName)
{
    this->m_identifierName = std::move(identifierName);
}

void CShaderParameter::setName (std::string name)
{
    this->m_name = std::move(name);
}