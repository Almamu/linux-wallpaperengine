#include "CShaderParameterFloat.h"

using namespace WallpaperEngine::Render::Shaders::Parameters;

CShaderParameterFloat::CShaderParameterFloat(irr::f32 defaultValue) :
    m_defaultValue (defaultValue),
    m_value (0),
    CShaderParameter (&this->m_defaultValue, nullptr, Type)
{

}

void CShaderParameterFloat::setValue (irr::f32 value)
{
    this->m_value = value;

    CShaderParameter::setValue (&this->m_value);
}

int CShaderParameterFloat::getSize ()
{
    return 1;
}

const std::string CShaderParameterFloat::Type = "float";