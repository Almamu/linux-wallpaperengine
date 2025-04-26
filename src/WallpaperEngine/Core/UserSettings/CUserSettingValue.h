#pragma once

#include <nlohmann/json.hpp>

#include "WallpaperEngine/Core/DynamicValues/CDynamicValue.h"

namespace WallpaperEngine::Core::UserSettings {
using namespace WallpaperEngine::Core::DynamicValues;

class CUserSettingValue : public CDynamicValue {
  public:
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

  protected:
    virtual ~CUserSettingValue() = default;
};
} // namespace WallpaperEngine::Core::UserSettings
