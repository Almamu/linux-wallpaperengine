#pragma once
#include <glm/vec3.hpp>

#include "WallpaperEngine/Render/Shaders/Variables/CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableVector4 : public CShaderVariable
    {
    public:
        explicit CShaderVariableVector4 (const glm::vec3& defaultValue);
        CShaderVariableVector4 (const glm::vec3& defaultValue, std::string name);

        const int getSize () const override;

        void setValue (const glm::vec3& value);

        static const std::string Type;

    private:
        glm::vec3 m_defaultValue;
        glm::vec3 m_value;
    };
}
