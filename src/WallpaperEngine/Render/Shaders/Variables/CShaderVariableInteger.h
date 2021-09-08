#pragma once

#include "CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables
{
    class CShaderVariableInteger : public CShaderVariable
    {
    public:
        explicit CShaderVariableInteger (int32_t defaultValue);
        CShaderVariableInteger (int32_t defaultValue, std::string name);

        const int getSize () const override;

        static const std::string Type;

        void setValue (int32_t value);

    private:
        int32_t m_defaultValue;
        int32_t m_value;
    };
}
