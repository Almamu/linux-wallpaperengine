#include "CShaderParameterInteger.h"

using namespace WallpaperEngine::Render::Shaders::Parameters;

CShaderParameterInteger::CShaderParameterInteger(irr::s32 defaultValue) :
    m_defaultValue (defaultValue),
    m_value (0),
    CShaderParameter (&this->m_defaultValue, nullptr, Type)
{

}

void CShaderParameterInteger::setValue (irr::s32 value)
{
    this->m_value = value;
    CShaderParameter::setValue (&this->m_value);
}

const int CShaderParameterInteger::getSize () const
{
    return 1;
}

const std::string CShaderParameterInteger::Type = "int";