#pragma once

#include <nlohmann/json.hpp>

namespace WallpaperEngine::Core::UserSettings
{
    class CUserSettingValue
    {
    public:
        template<class T> const T* as () const { assert (is <T> ()); return (const T*) this; }
        template<class T> T* as () { assert (is <T> ()); return (T*) this; }

        template<class T> bool is () { return this->m_type == T::Type; }

    protected:
        CUserSettingValue (std::string type);

    private:
        std::string m_type;
    };
};
