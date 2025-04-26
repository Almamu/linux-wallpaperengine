#pragma once

#include "WallpaperEngine/Core/DynamicValues/CDynamicValue.h"
#include <string>

namespace WallpaperEngine::Render::Shaders::Variables {
class CShaderVariable : public Core::DynamicValues::CDynamicValue {
  public:
    virtual ~CShaderVariable () = default;

    template <class T> [[nodiscard]] const T* as () const {
        if (is <T> ()) {
            return static_cast <const T*> (this);
        }

        throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] T* as () {
        if (is <T> ()) {
            return static_cast <T*> (this);
        }

        throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] bool is () const {
        return typeid (*this) == typeid(T);
    }

    [[nodiscard]] const std::string& getIdentifierName () const;
    [[nodiscard]] const std::string& getName () const;

    void setIdentifierName (std::string identifierName);
    void setName (const std::string& name);

  private:
    std::string m_identifierName = "";
    std::string m_name = "";
};
} // namespace WallpaperEngine::Render::Shaders::Variables
