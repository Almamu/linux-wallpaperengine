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
    explicit CShaderConstant (std::string type);

    template <class T> [[nodiscard]] const T* as () const {
        assert (is<T> ());
        return reinterpret_cast<const T*> (this);
    }

    template <class T> [[nodiscard]] T* as () {
        assert (is<T> ());
        return reinterpret_cast<T*> (this);
    }

    template <class T> [[nodiscard]] bool is () const {
        return this->m_type == T::Type;
    }

    /**
     * @return The type name of this constant
     */
    [[nodiscard]] const std::string& getType () const;

    /**
     * @return String representation of the constant's value
     */
    [[nodiscard]] virtual std::string toString () const = 0;

  private:
    const std::string m_type;
};
} // namespace WallpaperEngine::Core::Objects::Effects::Constants
