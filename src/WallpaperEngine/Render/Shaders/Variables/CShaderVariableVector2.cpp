#include "CShaderVariableVector2.h"

#include <utility>

using namespace WallpaperEngine::Render::Shaders::Variables;

CShaderVariableVector2::CShaderVariableVector2 (const glm::vec2& defaultValue) :
    m_defaultValue (defaultValue),
    m_value (glm::vec2 ()),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
}

CShaderVariableVector2::CShaderVariableVector2 (const glm::vec2& defaultValue, std::string name) :
    m_defaultValue (defaultValue),
    m_value (glm::vec2 ()),
    CShaderVariable (&this->m_defaultValue, nullptr, Type)
{
    this->setName (std::move(name));
}


void CShaderVariableVector2::setValue (const glm::vec2& value)
{
    this->m_value = value;

    CShaderVariable::setValue (&this->m_value);
}

const int CShaderVariableVector2::getSize () const
{
    return 2;
}

const std::string CShaderVariableVector2::Type = "vec2";