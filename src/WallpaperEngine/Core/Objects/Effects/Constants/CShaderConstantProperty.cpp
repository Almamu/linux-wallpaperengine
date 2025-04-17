#include "CShaderConstantProperty.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantProperty::CShaderConstantProperty (const CProperty* property) :
    CShaderConstant (Type),
    m_property (property) {}

const CProperty* CShaderConstantProperty::getProperty () const {
    return this->m_property;
}

std::string CShaderConstantProperty::toString () const {
    return "no string representation yet!";
}

const std::string CShaderConstantProperty::Type = "property";