#pragma once

#include <stdexcept>

namespace WallpaperEngine::Data::Utils {
/**
 * A simple implementation of type and sub-types casting to be as explicit
 * as possible while being performant
 */
class TypeCaster {
public:
    virtual ~TypeCaster () = default;

    template <class T> [[nodiscard]] const T* as () const {
	if (auto* ptr = dynamic_cast<const T*> (this)) {
	    return ptr;
	}

	throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] T* as () {
	if (auto* ptr = dynamic_cast<T*> (this)) {
	    return ptr;
	}

	throw std::bad_cast ();
    }

    template <class T> [[nodiscard]] T* asOrNull () { return dynamic_cast<T*> (this); }

    template <class T> [[nodiscard]] bool is () const { return dynamic_cast<const T*> (this) != nullptr; }
};
} // namespace WallpaperEngine::Data::Utils
