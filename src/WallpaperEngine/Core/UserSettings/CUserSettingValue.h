#pragma once

#include <nlohmann/json.hpp>

#include "WallpaperEngine/Core/DynamicValues/CDynamicValue.h"

namespace WallpaperEngine::Core::UserSettings {
using namespace WallpaperEngine::Core::DynamicValues;

class CUserSettingValue : public CDynamicValue {
  public:
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

  protected:
    explicit CUserSettingValue (std::string type);

  private:
    const std::string m_type;
};
} // namespace WallpaperEngine::Core::UserSettings
