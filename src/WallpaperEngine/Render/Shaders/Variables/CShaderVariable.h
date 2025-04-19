#pragma once

#include "WallpaperEngine/Core/DynamicValues/CDynamicValue.h"
#include <string>

namespace WallpaperEngine::Render::Shaders::Variables {
class CShaderVariable : public Core::DynamicValues::CDynamicValue {
  public:
    CShaderVariable (std::string type);
    virtual ~CShaderVariable () = default;

    template <class T> [[nodiscard]] const T* as () const {
        assert (is<T> ());
        return reinterpret_cast<const T*> (this);
    }

    template <class T> [[nodiscard]] T* as () {
        assert (is<T> ());
        return reinterpret_cast<T*> (this);
    }

    template <class T> [[nodiscard]] bool is () {
        return this->m_type == T::Type;
    }

    [[nodiscard]] const std::string& getIdentifierName () const;
    [[nodiscard]] const std::string& getName () const;
    [[nodiscard]] const std::string& getType () const;

    void setIdentifierName (std::string identifierName);
    void setName (const std::string& name);

  private:
    std::string m_identifierName;
    std::string m_name;
    std::string m_type;
};
} // namespace WallpaperEngine::Render::Shaders::Variables
