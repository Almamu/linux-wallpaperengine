#pragma once

#include "WallpaperEngine/Data/Utils/TypeCaster.h"
#include "WallpaperEngine/Data/Model/DynamicValue.h"
#include <exception>
#include <string>

namespace WallpaperEngine::Render::Shaders::Variables {
using namespace WallpaperEngine::Data::Model;
using namespace WallpaperEngine::Data::Utils;

class ShaderVariable : public DynamicValue, public TypeCaster {
  public:
    using DynamicValue::DynamicValue;

    [[nodiscard]] const std::string& getIdentifierName () const;
    [[nodiscard]] const std::string& getName () const;

    void setIdentifierName (std::string identifierName);
    void setName (const std::string& name);

  private:
    std::string m_identifierName;
    std::string m_name;
};
} // namespace WallpaperEngine::Render::Shaders::Variables
