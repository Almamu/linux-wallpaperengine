#pragma once

#include <cassert>
#include <string>
#include "WallpaperEngine/Core/DynamicValues/CDynamicValue.h"

namespace WallpaperEngine::Core::Objects::Effects::Constants {
/**
 * Shader constants base class
 */
class CShaderConstant : public DynamicValues::CDynamicValue {
  public:
    virtual ~CShaderConstant () = default;

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

    /**
     * @return String representation of the constant's value
     */
    [[nodiscard]] virtual std::string toString () const = 0;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
