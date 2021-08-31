#pragma once

#include "CShaderConstant.h"

#include <string>

namespace WallpaperEngine::Core::Objects::Effects::Constants
{
    class CShaderConstantFloat : public CShaderConstant
    {
    public:
        CShaderConstantFloat (float value);

        float* getValue ();

        static const std::string Type;
    protected:
        float m_value;
    };
}
