#pragma once

#include "CShaderConstant.h"

#include <string>
#include <glm/vec3.hpp>

namespace WallpaperEngine::Core::Objects::Effects::Constants
{
    class CShaderConstantVector2 : public CShaderConstant
    {
    public:
        CShaderConstantVector2 (glm::vec2 value);

        glm::vec2* getValue ();

        static const std::string Type;
    protected:
        glm::vec2 m_value;
    };
}
