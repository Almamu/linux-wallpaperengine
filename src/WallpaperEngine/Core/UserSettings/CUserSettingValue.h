#pragma once

#include <nlohmann/json.hpp>

namespace WallpaperEngine::Core::UserSettings {
class CUserSettingValue {
  public:
    template <class T> const T* as () const {
        assert (is<T> ());
        return reinterpret_cast<const T*> (this);
    }

    template <class T> T* as () {
        assert (is<T> ());
        return reinterpret_cast<T*> (this);
    }

    template <class T> bool is () const {
        return this->m_type == T::Type;
    }

  protected:
    explicit CUserSettingValue (std::string type);

  private:
    const std::string m_type;
};
} // namespace WallpaperEngine::Core::UserSettings
