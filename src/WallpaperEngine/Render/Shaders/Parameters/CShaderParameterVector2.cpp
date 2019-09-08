#include "CShaderParameterVector2.h"

using namespace WallpaperEngine::Render::Shaders::Parameters;

CShaderParameterVector2::CShaderParameterVector2 (const irr::core::vector2df& defaultValue) :
    m_defaultValue (defaultValue),
    m_value (irr::core::vector2df ()),
    CShaderParameter (&this->m_defaultValue, nullptr, Type)
{

}

void CShaderParameterVector2::setValue (irr::core::vector2df value)
{
    this->m_value = value;

    CShaderParameter::setValue (&this->m_value);
}

int CShaderParameterVector2::getSize ()
{
    return 2;
}

const std::string CShaderParameterVector2::Type = "vec2";