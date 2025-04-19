#include "WallpaperEngine/Core/DynamicValues/CDynamicValue.h"
#include "CShaderConstantProperty.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantProperty::CShaderConstantProperty (const CProperty* property) :
    CShaderConstant (Type) {
    property->connectOutgoing (this);
}

std::string CShaderConstantProperty::toString () const {
    return "no string representation yet!";
}

const std::string CShaderConstantProperty::Type = "property";