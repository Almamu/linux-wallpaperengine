#pragma once
#include <glm/vec4.hpp>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableVector4 : public CShaderVariable
    {
    public:
        explicit CShaderVariableVector4 (const glm::vec4& defaultValue);
        CShaderVariableVector4 (const glm::vec4& defaultValue, std::string name);

        const int getSize () const override;

        void setValue (const glm::vec4& value);

        static const std::string Type;

    private:
        glm::vec4 m_defaultValue;
        glm::vec4 m_value;
    };
}
