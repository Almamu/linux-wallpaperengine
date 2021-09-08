#pragma once

#include "CShaderConstant.h"

#include <string>
#include <glm/vec3.hpp>

namespace WallpaperEngine::Core::Objects::Effects::Constants
{
    class CShaderConstantVector3 : public CShaderConstant
    {
    public:
        CShaderConstantVector3 (glm::vec3 value);

        glm::vec3* getValue ();

        static const std::string Type;
    protected:
        glm::vec3 m_value;
    };
}
