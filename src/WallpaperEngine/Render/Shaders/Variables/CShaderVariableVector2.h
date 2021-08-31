#pragma once
#include <glm/vec2.hpp>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableVector2 : public CShaderVariable
    {
    public:
        explicit CShaderVariableVector2 (const glm::vec2& defaultValue);
        CShaderVariableVector2 (const glm::vec2& defaultValue, std::string name);

        const int getSize () const override;

        void setValue (const glm::vec2& value);

        static const std::string Type;

    private:
        glm::vec2 m_defaultValue;
        glm::vec2 m_value;
    };
}
