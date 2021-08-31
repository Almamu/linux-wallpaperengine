#pragma once

#include "CShaderConstant.h"

#include <string>

namespace WallpaperEngine::Core::Objects::Effects::Constants
{
    class CShaderConstantInteger : public CShaderConstant
    {
    public:
        CShaderConstantInteger (int32_t value);

        int32_t* getValue ();

        static const std::string Type;
    protected:
        int32_t m_value;
    };
}
