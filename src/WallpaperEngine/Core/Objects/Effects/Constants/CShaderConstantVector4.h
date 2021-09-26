#pragma once

#include "CShaderConstant.h"

#include <string>
#include <glm/vec4.hpp>

namespace WallpaperEngine::Core::Objects::Effects::Constants
{
    class CShaderConstantVector4 : public CShaderConstant
    {
    public:
        CShaderConstantVector4 (glm::vec4 value);

        glm::vec4* getValue ();

        static const std::string Type;
    protected:
        glm::vec4 m_value;
    };
}
