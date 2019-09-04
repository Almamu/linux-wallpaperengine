#pragma once

#include <nlohmann/json.hpp>

namespace WallpaperEngine::Core::Projects
{
    using json = nlohmann::json;

    class CPropertyColor;

    class CProperty
    {
    public:
        static CProperty* fromJSON (json data, const std::string& name);

        template<class T> const T As () const { assert (Is<T> ()); return (const T*) this; }
        template<class T> T As () { assert (Is<T> ()); return (T*) this; }

        template<class T> bool Is () { return this->m_type == T::Type; }

        std::string& getName ();
        std::string& getType ();
        std::string& getText ();
    protected:
        CProperty (std::string name, std::string type, std::string text);

        std::string m_type;
        std::string m_name;
        std::string m_text;
    };
}
