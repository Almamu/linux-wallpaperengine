#include "CShaderParameterVector3.h"

using namespace WallpaperEngine::Render::Shaders::Parameters;

CShaderParameterVector3::CShaderParameterVector3 (const irr::core::vector3df& defaultValue) :
    m_defaultValue (defaultValue),
    m_value (irr::core::vector3df ()),
    CShaderParameter (&this->m_defaultValue, nullptr, Type)
{

}

void CShaderParameterVector3::setValue (irr::core::vector3df value)
{
    this->m_value = value;
    CShaderParameter::setValue (&this->m_value);
}

int CShaderParameterVector3::getSize ()
{
    return 3;
}

const std::string CShaderParameterVector3::Type = "vec3";