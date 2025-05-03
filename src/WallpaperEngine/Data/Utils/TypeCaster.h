#pragma once

#include <stdexcept>

namespace WallpaperEngine::Data::Utils {
/**
 * A simple implementation of type and sub-types casting to be as explicit
 * as possible while being performant
 *
 * Uses typeid from C++17, so base types must have a virtual, default destructor
 */
class TypeCaster {
  public:
    virtual ~TypeCaster() = default;

    template <class T> [[nodiscard]] const T* as () const {
        if (is<T> ()) {
            return static_cast <const T*> (this);
        }

        throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] T* as () {
        if (is<T> ()) {
            return static_cast <T*> (this);
        }

        throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] bool is () const {
        return typeid(*this) == typeid(T);
    }
};
} // namespace WallpaperEngine::Data::Utils
