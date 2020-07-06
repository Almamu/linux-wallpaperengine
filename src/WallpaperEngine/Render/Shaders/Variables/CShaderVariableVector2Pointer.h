#pragma once

#include "CShaderVariable.h"

#include <irrlicht/irrlicht.h>

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableVector2Pointer : public CShaderVariable
    {
    public:
        CShaderVariableVector2Pointer (irr::core::vector2df* value);
        CShaderVariableVector2Pointer (irr::core::vector2df* value, std::string name);

        const int getSize () const override;

        static const std::string Type;
    };
}
