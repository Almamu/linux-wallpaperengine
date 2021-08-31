#pragma once

#include <glm/vec2.hpp>
#include "CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableVector2Pointer : public CShaderVariable
    {
    public:
        CShaderVariableVector2Pointer (glm::vec2* value);
        CShaderVariableVector2Pointer (glm::vec2* value, std::string name);

        const int getSize () const override;

        static const std::string Type;
    };
}
