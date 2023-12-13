#pragma once

#include "CShaderVariable.h"

namespace WallpaperEngine::Render::Shaders::Variables {
class CShaderVariableFloat final : public CShaderVariable {
  public:
    explicit CShaderVariableFloat (float defaultValue);
    CShaderVariableFloat (float defaultValue, const std::string& name);

    const int getSize () const override;

    void setValue (float value);

    static const std::string Type;

  private:
    float m_defaultValue;
    float m_value;
};
} // namespace WallpaperEngine::Render::Shaders::Variables
