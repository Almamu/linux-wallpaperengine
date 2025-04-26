#include "WallpaperEngine/Core/DynamicValues/CDynamicValue.h"
#include "CShaderConstantProperty.h"

using namespace WallpaperEngine::Core::Objects::Effects::Constants;

CShaderConstantProperty::CShaderConstantProperty (std::shared_ptr <const CProperty> property) {
    property->connectOutgoing (this);
}

std::string CShaderConstantProperty::toString () const {
    return "no string representation yet!";
}
