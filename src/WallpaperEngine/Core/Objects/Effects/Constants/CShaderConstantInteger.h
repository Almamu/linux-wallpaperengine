#pragma once

#include "CShaderConstant.h"

#include <string>

namespace WallpaperEngine::Core::Objects::Effects::Constants
{
    /**
     * Shader constant of type integer
     */
    class CShaderConstantInteger : public CShaderConstant
    {
    public:
        explicit CShaderConstantInteger (int32_t value);

        /**
         * @return A pointer to the actual value of the constant
         */
        int32_t* getValue ();

        /**
         * Type string indicator
         */
        static const std::string Type;
    protected:
        /** The constant's value */
        int32_t m_value;
    };
}
